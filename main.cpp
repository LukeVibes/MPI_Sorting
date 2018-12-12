
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
void  allocateInitalMemory(){
	int n = numberOfValuesToSort;
	
	valuesToSort = (int *)malloc(sizeof(int) * n); 
}

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



int main(int argc, char *argv[])

{
  int rank;
  int p;
  double wtime;
  
  //Setup
  MPI::Init(argc, argv);          
  p = MPI::COMM_WORLD.Get_size(); 
  rank = MPI::COMM_WORLD.Get_rank();
  
  
  
	string inputfile = "input-0" + to_string(rank) + ".txt";
	string outputfile = "output-0" + to_string(rank) + ".txt";
	
	cout << inputfile << endl;
	if(debug==true){cout << "Input File: " << rank  << endl;}
	importFromFile(inputfile);
	
	if(debug==true){cout << endl;}
	
	if(debug==true){cout << "Output File: "<< rank << endl;}
	exportToFile(outputfile);

  
  
  //Exit
  MPI::Finalize();
  return 0;
}

