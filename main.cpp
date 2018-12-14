
# include <cstdlib>
# include <iostream>
# include <fstream> 
# include <cilk/cilk.h>
# include <vector>
# include <string>
# include <math.h>  
# include <algorithm>
# include "mpi.h"
using namespace std;

///Inital values
int numberOfValuesToSort;
int numberOfProcessorsToUse;

///main array of inputted values
int* valuesToSort;

///p-sample related arrays
int* p_sample;
int* all_p_samples;
int* global_p;

///bucket related arrays
int* buckets;
int* bucketSize;

///debug values
bool debug=true;

///allocateInitalMemory: simply does most of malloc needed for program.
void  allocateInitalMemory(){
	int n = numberOfValuesToSort;
	int p = numberOfProcessorsToUse;
	
	valuesToSort = (int *)malloc(sizeof(int) * n); 
	p_sample	 = (int *)malloc(sizeof(int) * (p*p));
	all_p_samples= (int *)malloc(sizeof(int) * (p*p));
	global_p     = (int *)malloc(sizeof(int) * (p*p));
	buckets		 = (int *)malloc(sizeof(int) * n);
	bucketSize   = (int *)calloc(p, sizeof(int)); 
}

///importFromFile: simply imports text file (in specifc format) into array.
void importFromFile(string filename){
	
	ifstream inputFile;
	inputFile.open(filename);

	if (!inputFile) {
		cout << "FILE ERROR: importFromFile()" << endl;
		return;
	}
	
	///First save inital data
	inputFile >> numberOfValuesToSort;
	inputFile >> numberOfProcessorsToUse;
	
	///Allocate memory given above data
	allocateInitalMemory();
	
	///Next save the values to sort
	int v;
	int n = numberOfValuesToSort;
	for (int i = 0; i < n; i++) {
		inputFile >> v;
		valuesToSort[i] = (int) v;
	}
		
	inputFile.close();
}
  
///exportToFile: simply prints sorted array and performance data.
void exportToFile(string filename){
	
	fstream outputFile;
	outputFile.open("output.txt", ios::out | ios::trunc);
	
	///Print sorted array to file
	int n = numberOfValuesToSort;
	for(int i=0; i<n; i++){
		outputFile << valuesToSort[i];
		outputFile << endl;
	}
	outputFile << endl << endl;
	
	///Print performance data
	outputFile << "Performance: " << endl;
	
	
	
	outputFile.close();
}

///maxHeapifier(): recursive function that ensure value at location i follows,
///				   Heap Sort rules, and adjusts accordingly until array is in
///				   proper format.
void maxHeapifer(int* arr, int index, int n){
	int i_leftnode = (index * 2) +1;
	int i_rightnode = (index * 2) +2;
	int i_largest = index;
	
	///See if child is larger than parent, if so, record the larger child
	if((i_leftnode <= n) and (arr[i_leftnode] > arr[index])){
		i_largest = i_leftnode;
	}
	
	if((i_rightnode <= n) and (arr[i_rightnode] > arr[i_largest])){
		i_largest = i_rightnode;
	}
	
	///if a Heap Sort rule is violated adjust the array
	if(i_largest != index){
		int temp = arr[index];
		arr[index] = arr[i_largest];
		arr[i_largest] = temp;
		
		///because we swapped, we need to make sure values under the uzew
		///swapped value follow Heap Sort
		maxHeapifer(arr, i_largest, n);
	}
}

///maxHeapMaker(): creates the inital max heap needed for Heap Sort
void maxHeapMaker(int* arr, int n){
	for(int i= ceil(n/2)-1; i >= 0; i--){
		maxHeapifer(arr, i, n-1);
	}
}

///heapSorter(): the controller function to launch a Heap Sort
void heapSorter(int* arr, int n){
	maxHeapMaker(arr, n);
	
	///after making the max heap, perform the classic bottom-leaf swap and adjust
	int temp = -1;
	for(int i = n-1; i >= 0; i--){
		temp = arr[i];
		arr[i] = arr[0];
		arr[0] = temp;
		maxHeapifer(arr, 0, i-1);
	}
}

///p_sampleMaker(): gets every  n/pth value (n being the local n)
void p_samplerMaker(int* arr, int n, int p, int* output_p_sample){
	int p_index=0;
	int trigger = floor((n-p)/p);
	int count=0;
	
	for(int i = 0; i<n; i++){
		if(p_index==(p-1)){break;}///we only want to map all but the last value, as the last value 
								  /// will always be the last value in the input array (a.k.a. arr) 
		if(count == trigger){
			output_p_sample[p_index] = arr[i];
			p_index++;
			count = 0;		
		}
		else{
			count++;
		}
	}
	
	output_p_sample[p-1] = arr[n-1]; ///set the last value off p_sample to last value of input array
}

void bucketsPushoverValues(int ourBucket, int p, int bdisplace, int newvalue, int n){
	int index = bdisplace;
	int b = ourBucket;
	
	///Part 1: check if we even NEED to push values over (for an insert)
	///-we do this by seeing if the bucketSizes ahead of our bucket 
	/// is greater than zero, not including out bucketsize as we add to
	/// the end of our bucekt thanks to bdisplc including our buckets size!
	int valuesAhead=0;
	for(int i=b+1; i<p; i++){
		valuesAhead += bucketSize[i];
	}
	
	///Part 2: ok so if there are values ahead of us, slide em over
	if(valuesAhead>0 and index<(n-1)){
		int temp;
		int tempB;
		temp = buckets[index];
		buckets[index] = newvalue;
		
		///we [start] at index+1 because           : we already shift the inital spot aboce
		///we [ end ] at <valuesAhead+index because: we only have valuesAhead-1 values to go 
		for(int i=index+1; i<valuesAhead+index; i++){
			tempB = buckets[i];
			buckets[i] = temp;
			temp = tempB;
		}
	}
	
	///If their arent any values ahead of us, just add it!
	else{
		buckets[index] = newvalue;
	}
	
	cout << "buckets::: " << valuesToSort[index] << " i: " << index << endl;
		for(int i=0; i<n; i++){
			cout << buckets[i] << ", ";
		}
		cout << endl << endl;
}
//REMEMBER TO ADD: IF NOT SMALLER THAN FIRST 3 BUCKETS, PUT IN LAST BUCKET
void bucketMaker(int n, int p){
	
	int largestBucket=0;
	int loopStart=0;
	int bdisplc=0;
	for(int i=0; i<n; i++){
		
		loopStart = largestBucket;
		
		///Check which bucket it belongs in...
		for(int b=loopStart; b<p; b++){
			if(valuesToSort[i] <= global_p[b]){
				if (b>largestBucket){largestBucket=b;} ///No point in checking buckets we already filled,
													   ///we can do this thanks to the local array being
													   ///sorted!
				
				
				///To see where in buckets[] we place the new value we need to know,
				///works by getting your bucket size (so it goes to end) plus all bucket
				///sizes before you.			
				for(int q=b; q>=0; q--){
					bdisplc += bucketSize[q];
				}
				cout << endl;
				
				
				///pushover values and insert!
				bucketsPushoverValues(b, p, bdisplc, valuesToSort[i], n);
				bucketSize[b] = bucketSize[b] + 1;
				bdisplc=0;
				break;
			}
		}
	}

}

int main(int argc, char *argv[])

{
	int rank;
	int p;
	double wtime;

 //Setup
	MPI::Init(argc, argv);          
	p = MPI::COMM_WORLD.Get_size(); 
	rank = MPI::COMM_WORLD.Get_rank();


 //Step One: Locally Sort each processors values
	string inputfile = "input-0" + to_string(rank) + ".txt";
	string outputfile = "output-0" + to_string(rank) + ".txt";

	importFromFile(inputfile);
	int n = numberOfValuesToSort;
	heapSorter(valuesToSort, n);


 //Step Two: Create local p-sample
	p = numberOfProcessorsToUse;
	p_samplerMaker(valuesToSort, n, p, p_sample);
	
 //Step Three: Send all p-sample's to Proc-0
	///malloc room for all p samples on Proc-0
	MPI_Alltoall(p_sample, p, MPI_INT,
			     all_p_samples, p, MPI_INT, 
				 MPI_COMM_WORLD);
	
	if(rank==0){
		for(int i=0; i<(p*p); i++){
			cout << all_p_samples[i] << " ";
		}
		cout << endl;
	}

 //Step Four: Sort all p-samples in Proc-0
	if(rank==0){
		heapSorter(all_p_samples, p*p);
	}
	if(rank==0){
		for(int i=0; i<(p*p); i++){
			cout << all_p_samples[i] << " ";
		}
		cout << endl;
	}

 //Step Five: Create global-p in Proc-0
	if(rank==0){
		p_samplerMaker(all_p_samples, p*p, p, global_p);
	}
	if(rank==0){
		for(int i=0; i<(p); i++){
			cout << global_p[i] << " ";
		}
		cout << endl;
	}

 //Step Seven: Send global-p to all processors
	int* sendBuffer = (int *)malloc(sizeof(int) * p*p);
	if(rank==0){
		int j=0;
		for(int i=0; i<p*p; i++){
			if(i%p==0){j=0;}
			
			sendBuffer[i]= global_p[j];
			j++;
			
		}
	}
	MPI_Alltoall(sendBuffer, p, MPI_INT,
			     global_p, p, MPI_INT, 
				 MPI_COMM_WORLD);
	
	if(rank==2){
		for(int i=0; i<(p); i++){
			cout << global_p[i] << " ";
		}
		cout << endl;
	}

//Step Eight: Locally Create p buckets
	///create buckets and bucketSize
	bucketMaker(n, p);
		
	///create sdisplc array

//Step Nine: Send bucket sizes

	///create recvcount
	///create rdisplc

//Step Ten: bucket Alltoallv









	/*
		cout << endl << endl;
		cout << "global-p: " << endl;
		for(int i=0; i<(p); i++){
			cout << global_p[i] << ", ";
		}
		cout << endl << endl;
		
		for(int b=0; b<p; b++){
			cout << "Bucket " << b << ":" << endl;
			cout << "    size: " << bucketSize[b] << endl << endl;
		}
		
		cout << endl << endl;
		
		cout << "buckets: " << endl;
		for(int i=0; i<n; i++){
			cout << buckets[i] << ", ";
		}
		cout << endl << endl;
		
		cout << "valuesToSort: " << endl;
		for(int i=0; i<n; i++){
			cout << valuesToSort[i] << ", ";
		}
		cout << endl << endl;
		*/
	
	
	/*
	MPI_Barrier(MPI_COMM_WORLD);
	if(rank==0){
		for(int i=0; i<p; i++){
		if(debug==true){cout << p_sample[i] << " ";}
		}
		
		if(debug==true){cout << endl;}
		for(int i=0; i<n; i++){
		if(debug==true){cout << valuesToSort[i] << " ";}
		}
		if(debug==true){cout << endl << endl;}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	if( rank==1){
		for(int i=0; i<p; i++){
		if(debug==true){cout << p_sample[i] << " ";}
		}
	
		if(debug==true){cout << endl;}
		for(int i=0; i<n; i++){
		if(debug==true){cout << valuesToSort[i] << " ";}
		}
		if(debug==true){cout << endl << endl;}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	if( rank==2){
		for(int i=0; i<p; i++){
		if(debug==true){cout << p_sample[i] << " ";}
		}
		if(debug==true){cout << endl << endl;}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	if( rank==3){
		for(int i=0; i<p; i++){
		if(debug==true){cout << p_sample[i] << " ";}
		}
		if(debug==true){cout << endl << endl;}
	}
	*/


	exportToFile(outputfile);


	//Exit
	free(bucketSize);
	//free(recvcount);
	//free(sdisplc);
	//free(rdisplc)
	free(buckets);
	//free(sendBuffer);
	free(global_p);
	free(all_p_samples);
	free(p_sample);
	free(valuesToSort);
	MPI::Finalize();
	return 0;
}

