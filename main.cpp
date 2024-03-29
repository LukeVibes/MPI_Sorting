
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
int* sdisplc;
int* rdisplc;
int* recvcount;
int* post_buckets;
int* procs_post_bucket_sizes;
int* final_bucket;

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
	sdisplc      = (int *)calloc(p, sizeof(int)); 
	rdisplc		 = (int *)calloc(p, sizeof(int)); 
	recvcount    = (int *)malloc(sizeof(int) * p);
	procs_post_bucket_sizes = (int *)malloc(sizeof(int) * p);
	final_bucket = (int *)malloc(sizeof(int) * (n*p));
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
	outputFile.open(filename, ios::out | ios::trunc);
	
	///Print sorted array to file
	int n = numberOfValuesToSort;
	for(int i=0; i<n; i++){
		outputFile << final_bucket[i];
		outputFile << endl;
	}
	outputFile << endl << endl;
	
	///Print performance data
	outputFile << "Performances: " << endl;
	
	
	
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
				
				///pushover values and insert!
				bucketsPushoverValues(b, p, bdisplc, valuesToSort[i], n);
				bucketSize[b] = bucketSize[b] + 1;
				bdisplc=0;
				break;
			}
		}
	}

}

void displcMaker(int p, int* displc, int* sizes){
	
	int total;
	for(int i=0; i<p; i++){
		total=0;
		
		if(i>0){
			for(int b=i-1; b>=0; b--){
				total += sizes[b];
			}
		}
		displc[i] = total;
	}
}

void arrayAllNegOne(int* arr, int n, int rank){

	for(int i=0; i<n; i++){
		arr[i] = 0;
	}
}

void sendValuesback(int proc, int n, int p, int oversize){
	int valuesToMove = oversize - n;
	
	for(int i=(n-1); i<oversize; i++){
		post_buckets[i];
	}
}

void sendValuesForward(int proc, int n, int p, int oversize){
}

int main(int argc, char *argv[])

{
	int rank;
	int p;
	double wtime;

 //Set-up
	MPI::Init(argc, argv);          
	p = MPI::COMM_WORLD.Get_size(); 
	rank = MPI::COMM_WORLD.Get_rank();


 //Step One: Locally Sort each processors values
	string inputfile = "input-0" + to_string(rank) + ".txt";

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
	displcMaker(p, sdisplc, bucketSize);
	
	
	
	MPI_Barrier(MPI_COMM_WORLD);
	if(rank==1){
		cout << "rank "<< rank << endl;
		for(int b=0; b<p; b++){
			cout << "bucket " << b << ": ";
			for(int i=0; i<bucketSize[b]; i++){
					int plus=0;
					if(b==0){plus=0;}
					else{plus = sdisplc[b];}
					cout << buckets[plus+i] << ", ";
			}
				cout << endl;
		}
		
	}
	
	
	
//Step Nine: Send bucket sizes and prep bucket sending
	///sending (also creates recvcount aka bucket sizes of others)
	MPI_Alltoall(bucketSize, 1, MPI_INT,
				 recvcount, 1, MPI_INT,
				 MPI_COMM_WORLD);
	
	///create rdisplc
	displcMaker(p, rdisplc, recvcount);
	
	///create post_buckets (with custom size)
	int post_buckets_size=0;
	for(int i=0; i<p; i++){post_buckets_size += recvcount[i];}
	post_buckets = (int *)malloc(sizeof(int) * post_buckets_size);
	
	///set post_buckets with all -1's for testing purposes...
	//arrayAllNegOne(post_buckets, n, rank);

//Step Ten: bucket Alltoallv
	MPI_Alltoallv(buckets, bucketSize, sdisplc, MPI_INT,
				  post_buckets, recvcount, rdisplc, MPI_INT,
				  MPI_COMM_WORLD);
	
	

//Step Eleven: Sort post_buckets
	heapSorter(post_buckets, post_buckets_size);
	
//Step Twelve: send out post-bucket sizes to all
	
	///create a send buffer...
	int* sendbuf = (int *)malloc(sizeof(int) * p);
	for(int i=0; i<p; i++){sendbuf[i]= post_buckets_size;} ///post_buckets_size created in Step Nine
	
	///sending
	MPI_Alltoall(sendbuf, 1, MPI_INT,
				 procs_post_bucket_sizes, 1, MPI_INT,
				 MPI_COMM_WORLD);
				 
				 
				 
	
	


//Step Thirteen: Adjust post-bucket sizes
	///prep allToAllv values for non Proc-0 processors
	if(rank != 0){
		for(int i=0; i<p; i++){
			sdisplc[i]=0;	
			recvcount[i]=0;
			rdisplc[i]=0;							
		}
		
		bucketSize[0] = post_buckets_size;	///post_buckets_size created in Step Nine
		for(int i=1; i<p; i++){
			bucketSize[i]=0;								
		}
	}
	
	///prep alltoAllv values for Proc-0
	if(rank == 0){	
		for(int i=0; i<p; i++){
			cout << procs_post_bucket_sizes[i] << ", ";
		}
		cout << endl;
		
		rdisplc[0]=0;
		bucketSize[0] = procs_post_bucket_sizes[0];
		for(int i=0; i<p; i++){
			recvcount[i] = procs_post_bucket_sizes[i];
			if(i>0){
				bucketSize[i]=0;
			}	
		}
		
		displcMaker(p, rdisplc, procs_post_bucket_sizes);
	}
	
	
	int* recvbuf = (int *)malloc(sizeof(int) * (n*p));
	int* bucketbuf = (int *)malloc(sizeof(int) * (n*p));
	for(int i=0; i<procs_post_bucket_sizes[rank]; i++){bucketbuf[i]=post_buckets[i];}
	
	///send all values to processor one
	MPI_Alltoallv(bucketbuf, bucketSize, sdisplc, MPI_INT,
				  recvbuf, recvcount, rdisplc, MPI_INT,
				  MPI_COMM_WORLD);
				  
	///prep to send back values, now evenly distriuted through processors.
	for(int i=0; i<p; i++){
		if(rank==0){
			bucketSize[i] = n; //IMPORANT: here n is the value we read from the inputfile upon starting
								//          the program. we want each processor to have this many values
								//		   in its output file as this MUST be the most effective outcome.
		}
		else{
			bucketSize[i] =0;
			sdisplc[i]=0;
		}
		if(i>0){rdisplc[i]=n;}
		else{rdisplc[0]=0;}      
		
		if(i>0){recvcount[i] = 0;}
		else{recvcount[0] = n;} //IMPORTANT: same logic as bucketSize[]
	}
	if(rank==0){displcMaker(p, sdisplc, bucketSize);}
	
	///send back values to processors
	MPI_Alltoallv(recvbuf, bucketSize, sdisplc,	MPI_INT	,
				  final_bucket, recvcount, rdisplc, MPI_INT,
				  MPI_COMM_WORLD);
	
//Step Fourteen: Print to output file!
	string outputfile = "output-0" + to_string(rank) + ".txt";
	exportToFile(outputfile);


 //Wrap-up
	free(bucketbuf);
	free(recvbuf);
	free(procs_post_bucket_sizes);
	free(sendbuf);
	free(post_buckets);
	free(bucketSize);
	free(recvcount);
	free(sdisplc);
	free(rdisplc);
	free(buckets);
	free(global_p);
	free(all_p_samples);
	free(p_sample);
	free(valuesToSort);
	
	MPI::Finalize();
	return 0;
}

