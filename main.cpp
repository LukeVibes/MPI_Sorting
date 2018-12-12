
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


int numberOfValuesToSort;
int numberOfProcessorsToUse;
int* valuesToSort;

bool debug=true;

///allocateInitalMemory: simply does most of malloc needed for program.
void  allocateInitalMemory(){
	int n = numberOfValuesToSort;
	
	valuesToSort = (int *)malloc(sizeof(int) * n); 
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
	
		if(debug==true){cout << valuesToSort[i] << " ";}
	}
	if(debug==true){cout << endl;}
		
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
		if(debug==true){cout << valuesToSort[i] << " ";}
	}
	outputFile << endl << endl;
	if(debug==true){cout << endl;}
	
	///Print performance data
	outputFile << "Performance: " << endl;
	
	
	
	outputFile.close();
}

///maxHeapifier(): recursive function that ensure value at location i follows,
///				   Heap Sort rules, and adjusts accordingly until array is in
///				   proper format.
void maxHeapifer(int index, int n){
	int i_leftnode = (index * 2) +1;
	int i_rightnode = (index * 2) +2;
	int i_largest = index;
	
	///See if child is larger than parent, if so, record the larger child
	if((i_leftnode <= n) and (valuesToSort[i_leftnode] > valuesToSort[index])){
		i_largest = i_leftnode;
	}
	
	if((i_rightnode <= n) and (valuesToSort[i_rightnode] > valuesToSort[i_largest])){
		i_largest = i_rightnode;
	}
	
	///if a Heap Sort rule is violated adjust the array
	if(i_largest != index){
		int temp = valuesToSort[index];
		valuesToSort[index] = valuesToSort[i_largest];
		valuesToSort[i_largest] = temp;
		
		///because we swapped, we need to make sure values under the uzew
		///swapped value follow Heap Sort
		maxHeapifer(i_largest, n);
	}
}

///maxHeapMaker(): creates the inital max heap needed for Heap Sort
void maxHeapMaker(int n){
	for(int i= ceil(n/2)-1; i >= 0; i--){
		maxHeapifer(i, n-1);
	}
}

///heapSorter(): the controller function to launch a Heap Sort
void heapSorter(int n){
	maxHeapMaker(n);
	
	///after making the max heap, perform the classic bottom-leaf swap and adjust
	int temp = -1;
	for(int i = n-1; i >= 0; i--){
		temp = valuesToSort[i];
		valuesToSort[i] = valuesToSort[0];
		valuesToSort[0] = temp;
		maxHeapifer	(0, i-1);
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
 
  if(rank==0){
	string inputfile = "input-0" + to_string(rank) + ".txt";
	string outputfile = "output-0" + to_string(rank) + ".txt";
	
	cout << inputfile << endl;
	if(debug==true){cout << "Input File: " << rank  << endl;}
	importFromFile(inputfile);
	
	if(debug==true){cout << endl;}
	int n = numberOfValuesToSort;
	heapSorter(n);
	
	if(debug==true){cout << "Output File: "<< rank << endl;}
	exportToFile(outputfile);

}
  
  //Exit
  MPI::Finalize();
  return 0;
}

