//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%%					High Performance Computing Lab, Indian Institute of Technology, Bombay(IITB)		%%
//%%										Powai, Mumbai,India												%%
//%=========================================================================================================%%
// %%This is the Intellectual Property of High Performance Computing Laboratory,IIT Bombay, and hence 		%%
// %%should not be used for any monetary benefits without the proper consent of the Institute. However		%%	
// %%it can be used as reference related to academic activities. In the event of publication				%%
// %%the following notice is applicable																		%% 
// %%Copyright(c) 2013 HPC Lab,IIT Bombay.																	%%
// %%The entire notice above must be reproduced on all authorized copies.									%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%% Project Name	: Project First Phase "SIRC and EDK Based Matrix Multiplication on GF(2)"						%% 
//%% File Name		: eth_SW_Example.cpp																		%%
//%% Title 		: SIRC Application Code for Matrix Multiplication															%%
//%% Author		: Ken Eguro, Sandro Forin,Jeebu Jacob Thomas																	%%
//%% Description	:																						%%
//%% Version		: 	1.0																					%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Author: Ken Eguro and Sandro Forin
// Includes code written by 2009 MSR intern Rene Mueller.
//
// Created: 10/23/09 
//
// Version: 1.1
// 
// Changelog:
// Updated to fix bugs and compile from .lib
//
// Description:
// Example test program.
// Test #1 - Read and write parameter registers
// Test #2 - Soft reset of user logic
// Test #3 - Test circuit:	send random input bytes to the FPGA 
//							multiply by 3
//							retrieve results
// Test #4 - Test circuit: same as test #3, only use new random numbers & write/execute command
// Test #5 - Test circuit: same as test #4, only show how to handle small output error of write/execute command
// Test #6 - Write bandwidth test
// Test #7 - Read bandwidth test
//----------------------------------------------------------------------------
//

#include <iostream>
#include <fstream>
#include <windows.h>
#include <WinIoctl.h>
#include <setupapi.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>
#include <assert.h>
#include <list>
#include <vector>
#include <time.h>
#include <direct.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include "sirc_internal.h"
#include "display.h"
#include <stdint.h>

#define N 1024		// this is the size of input matrices

//#define BUGHUNT 1  In case its needed again in the future.

using namespace std;
using namespace System;
using namespace System::IO;
using namespace System::Collections;

void error(string inErr)
{
	cerr << "Error:" << endl;
	cerr << "\t" << inErr << endl;
	int a;
	cin>>a;
    PrintZeLog();
	exit(-1);
}

DWORD dwDisplayThreadID = 0;
typedef char strbuf[64];
strbuf lines[4];
bool displayUpdated = false;

DWORD WINAPI displayThreadProc(LPVOID lpParam)
{
	SW_Example::display displayObj;
	//eth_sirc_lib_SW_Example::display displayObj;
	displayObj.Text = "SIRC Test Application";
	displayObj.Show();
	
	MSG tempMsg;
	while(1){
		if(::GetMessage(&tempMsg, NULL, 0, 0) > 0){
			if(tempMsg.message == WM_PAINT){
                String ^t0, ^t1, ^t2, ^t3;
                t0 = gcnew String(lines[0]);
                t1 = gcnew String(lines[1]);
                t2 = gcnew String(lines[2]);
                t3 = gcnew String(lines[3]);
				displayObj.updateStrings(t0, t1, t2, t3);
				displayUpdated = true;
			}
            ::TranslateMessage(&tempMsg); 
            ::DispatchMessage(&tempMsg);
            if ((tempMsg.message == WM_DESTROY) ||
                (tempMsg.message == WM_CLOSE) ||
                (tempMsg.message == WM_QUIT))
                break;
		}
		else{
			break;
		}
	}
    return 0;
}

int main(int argc, char* argv[])
{
	using namespace std;
	//Test harness variables
	ETH_SIRC *SIRC_P;
	uint8_t FPGA_ID[6];
	bool FPGA_ID_DEF = false;
	int a;
	uint32_t numOps = 0;
	uint32_t numOpsReturned;
	uint32_t tempInt;
	uint32_t artificialStopPoint;
   	uint32_t driverVersion = 0;
	//int sizewrite=(N/32)*(N/32)*(N/32);
	// int sizewrite=(N/32)*(N/32);
	
	int sizewrite=(N/64)*(N/64);
	double *writeTime=new double[sizewrite];
	
	//How many bytes do we want to send for each bandwidth test?
	//Default value = 8KB
	
	int BANDWIDTHTESTSIZE = 0;
	//How many times would we like to send this message?
	int BANDWIDTHTESTITER = 0;

	char *token = NULL;
	char *next_token = NULL;
	// uint32_t val;
	uint64_t val;
	int waitTimeOut = 0;

	//Input buffer
	//uint8_t *inputValues;
	// uint32_t *inputValues;
	uint64_t *inputValues;
	//Output buffer
	//uint8_t *outputValues;
	// uint32_t *outputValues;
	uint64_t *outputValues;

	//Speed testing variables
	DWORD start, end;

	std::ostringstream tempStream;

	DWORD dwDisplayThreadID;

	// uint32_t** matA = new uint32_t*[N];
	// uint32_t** matB = new uint32_t*[N];
	// uint32_t** matC = new uint32_t*[N];

	// uint32_t* countervalues = new uint32_t[(N/32)*(N/32)];
	
	uint64_t** matA = new uint64_t*[N];
	uint64_t** matB = new uint64_t*[N];
	uint64_t** matC = new uint64_t*[N];

	// uint32_t* countervalues = new uint32_t[(N/32)*(N/32)];
	uint64_t* countervalues = new uint64_t[(N/64)*(N/64)];
	int s = 0;
	for(int i = 0; i < N; ++i)
	{
	    // matA[i] = new uint32_t[N/32];
	    // matB[i] = new uint32_t[N/32];
	    // matC[i] = new uint32_t[N/32];
		matA[i] = new uint64_t[N/64];
	    matB[i] = new uint64_t[N/64];
	    matC[i] = new uint64_t[N/64];
	}

	cout << "Entering Matrix A "<< endl;  
	   // for(int i=0;i<N;i++){
	    // for(int j=0;j<N/32;j++){
	     // matA[i][j] = 0x00000008; 
	     // matC[i][j] = 0x00000000;			// initialize matrix C also    
	   //cout << matA[i][j];
	    // }
	   //cout << endl;}
	   for(int i=0;i<N;i++)
	   {
	    for(int j=0;j<N/64;j++)
		{
	     matA[i][j] = 0x0000000000000008; 
	     matC[i][j] = 0x0000000000000000;			// initialize matrix C also    
		 // cout << matA[i][j];
	    }
		 // cout << endl;
	   }

	cout << "Now entering Matrix B " <<endl;
	   // for(int i=0;i<N;i++){
	    // for(int j=0;j<N/32;j++){
	     // if(j==(i/32)) matB[i][j] = 0x80000000 >> (i%32);   
	     // else matB[i][j] = 0x00000000 ;  
	  // cout << matB[i][j] ;
	    // }
	    //cout << endl;
	   // }	   
	   for(int i=0;i<N;i++)
	   {
	    for(int j=0;j<N/64;j++)
		{
	     if(j==(i/64)) matB[i][j] = 0x8000000000000000 >> (i%64);   
	     else matB[i][j] = 0x0000000000000000 ;  
		 // cout << matB[i][j]<<hex; ;
	    }
		 // cout << endl;
	   }

#ifndef BUGHUNT
    StartLog();
#endif

	//**** Process command line input
	if (argc > 1) 
	{
		for(int i = 1; i < argc; i++)
		{
			if(strcmp(argv[i], "-mac") == 0)
			{ 
				if(argc <= i + 1)
				{
					tempStream << "-mac option requires argument";
					error(tempStream.str());					
				}
				
				//Map the target MAC address to an FPGA id.
				val = hexToFpgaId(argv[i + 1],FPGA_ID, sizeof(FPGA_ID));
				//Check to see if there were exactly 6 bytes
				if (val!=6) 
				{
					tempStream << "Invalid MAC address. macToFpgaId returned " << i;
					error(tempStream.str());
				}

				cout << "destination MAC: "
						<< hex << setw(2) << setfill('0') 
						<< setw(2) << (int)FPGA_ID[0] << ":" 
						<< setw(2) << (int)FPGA_ID[1] << ":" 
						<< setw(2) << (int)FPGA_ID[2] << ":" 
						<< setw(2) << (int)FPGA_ID[3] << ":" 
						<< setw(2) << (int)FPGA_ID[4] << ":" 
						<< setw(2) << (int)FPGA_ID[5] << dec << endl;
				FPGA_ID_DEF = true;
				i++;
			}
			else if (strcmp(argv[i], "-numOps") == 0)
			{
				if(argc <= i + 1)
				{
					tempStream << "-numOps option requires argument";
					error(tempStream.str());					
				}
				//Grab # of datapoints
				numOps = (uint32_t) atoi(argv[i + 1]);
				if ((numOps < 2))
				{
					tempStream << "Invalid number of operations: " << (int)numOps << ".  Must be > 1";
					error(tempStream.str());
				}
				i++;
			}
			else if (strcmp(argv[i], "-waitTimeOut") == 0)
			{
				if(argc <= i + 1)
				{
					tempStream << "-waitTime option requires argument";
					error(tempStream.str());					
				}
				waitTimeOut = (uint32_t) atoi(argv[i + 1]);
				if ((waitTimeOut < 1)) 
				{
					tempStream << "Invalid waitTime: " << waitTimeOut << ".  Must be >= 1";
					error(tempStream.str());
				}
				i++;
			}
			else if (strcmp(argv[i], "-bandwidthIter") == 0){
				if(argc <= i + 1)
				{
					tempStream << "-bandwidthIter option requires argument";
					error(tempStream.str());					
				}
				BANDWIDTHTESTITER = (uint32_t) atoi(argv[i + 1]);
				if ((BANDWIDTHTESTITER < 1)) 
				{
					tempStream << "Invalid BANDWIDTHTESTITER: " << BANDWIDTHTESTITER << ".  Must be >= 1";
					error(tempStream.str());
				}
				i++;
			}
			else
			{
				tempStream << "Unknown option: " << argv[i] << endl;
				tempStream << "Usage: " << argv[0] << " {-mac X:X:X:X:X:X} {-numOps X} {-waitTimeOut X} {-bandwidthIter X}" << endl;
				error(tempStream.str());
			}
		}
	} 
	if(!FPGA_ID_DEF)
	{
		cout << "****USING DEFAULT MAC ADDRESS - AA:AA:AA:AA:AA:AA" << endl;
		FPGA_ID[0] = 0xAA;
		FPGA_ID[1] = 0xAA;
		FPGA_ID[2] = 0xAA;
		FPGA_ID[3] = 0xAA;
		FPGA_ID[4] = 0xAA;
		FPGA_ID[5] = 0xAA;
	}

	//**** Set up communication with FPGA
	//Create communication object
	SIRC_P = new ETH_SIRC(FPGA_ID, driverVersion, NULL);
	//Make sure that the constructor didn't run into trouble
    if (SIRC_P == NULL)
	{
		tempStream << "Unable to find a suitable SIRC driver or unable to ";
		error(tempStream.str());
    }
	if(SIRC_P->getLastError() != 0)
	{
		tempStream << "Constructor failed with code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
	}
	
    //Get runtime parameters, for what we wont change.
    SIRC::PARAMETERS params;
    if (!SIRC_P->getParameters(&params,sizeof(params)))
	{
		tempStream << "Cannot getParameters from SIRC interface, code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
    }

    //These MUST match the buffer sizes in the hw design.
    params.maxInputDataBytes  = 1<<14; // should be > 8704 hence 2^14
    params.maxOutputDataBytes = 1<<14; // should be > 8192 hence 2^14

	if(waitTimeOut != 0)
	{
		params.writeTimeout = waitTimeOut;
		params.readTimeout = waitTimeOut;
	}

	if (!SIRC_P->setParameters(&params,sizeof(params)))
	{
		tempStream << "Cannot setParameters on SIRC interface, code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
    }
	
    //Fill up the input buffer
    if (numOps == 0)
	{
      // numOps = min(params.maxInputDataBytes, params.maxOutputDataBytes);
		
		numOps = 64*17; // we want to send 1088 -- 64 bit words
	}
	
	cout << "****Beginning test #3 - simple test application" << endl;
	
	//inputValues = (uint8_t *) malloc(sizeof(uint8_t) * numOps);
	inputValues = (uint64_t *) malloc(sizeof(uint64_t) * numOps);
	assert(inputValues);
	//outputValues = (uint8_t *) malloc(sizeof(uint8_t) * numOps);
	outputValues = (uint64_t *) malloc(sizeof(uint64_t) * numOps);
	assert(outputValues);


    LogIt(LOGIT_TIME_MARKER);
	//start = GetTickCount();

	 __int64 ctr1 = 0, ctr2 = 0, freq = 0;
	 
	//Set parameter register 0 to the number of operations
	if(!SIRC_P->sendParamRegisterWrite(0, numOps)){
		tempStream << "Parameter register write failed with code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
	}
	//Set parameter register 1 to a multiplier of 3
	if(!SIRC_P->sendParamRegisterWrite(1, 3)){
		tempStream << "Parameter register write failed with code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
	}

//QueryPerformanceCounter((LARGE_INTEGER *) &ctr1);
double realfpgatime = 0.0;


  //////////////////////////////
  for(int i=0;i<N/64;i++)
	{
		for(int j=0;j<N/64;j++)
			{


				  // Load block from matrix A
				for(int m=0;m<64;m++)
				  { 
					inputValues[m] = matA[i*64+m][j]; 
				  }

				  // Load block from matrix B
				for(int q=0;q<N/64;q++)
					{

					  for(int m=0;m<64;m++)
					  {
						inputValues[m+64+64*q] = matB[j*64+m][q];
					  }


				    }
				  
		
				 QueryPerformanceCounter((LARGE_INTEGER *) &ctr1);
				  //sendWrite, sendRun, sendRead
				  if(!SIRC_P->sendReset()){
						tempStream << "Reset failed with code " << (int) SIRC_P->getLastError();
						error(tempStream.str());
					}
				  
					uint32_t outputLength=(uint32_t) 16*16;
					
					//Next, send the input data
					//Start writing at address 0
					 
					if(!SIRC_P->sendWriteAndRun(0,8*numOps,(uint8_t *)inputValues,30000,(uint8_t *)outputValues,8*(numOps-64)+8,& outputLength))
					{ //+8 is for counter
						/////////////////8*numops because numops represents number of words and we are sending 8 byte words... so number of bytes to send = 8*numops
						cout<<"Here";
						cin>>a;
						tempStream << "Write to FPGA failed with code " << (int) SIRC_P->getLastError();
						error(tempStream.str());
					}
					
					// cout<<"Block "<< i*16+j << endl;
					// cout<<"HEre writing started"<<endl;
					
					// if(!SIRC_P->sendWrite(0,8*numOps,(uint8_t *)inputValues)){ 
						/////////// 4*numops because numops represents number of words and we are sending 4 byte words... so number of bytes to send = 4*numops
						
						// tempStream << "Write to FPGA failed with code " << (int) SIRC_P->getLastError();
						// error(tempStream.str());
					// }
					
						
						// cout<<"Write finished"<<endl;
						
						
					// Set the run signal
					// if(!SIRC_P->sendRun()){
						// tempStream << "Run command failed with code " << (int) SIRC_P->getLastError();
						// error(tempStream.str());
					// }

					
						// cout<<"Run initiated"<<endl;
					
					
					// Wait up to N seconds for the execution to finish (we can compute ~500M numbers in that time)
					// if(waitTimeOut == 0){
						// if(!SIRC_P->waitDone(300)){
							// cout<<"I am here\n";
							// tempStream << "Wait till done failed with code " << (int) SIRC_P->getLastError();
							// error(tempStream.str());
						// }
						// }

					
					// else{
						// if(!SIRC_P->waitDone(waitTimeOut)){
							// tempStream << "Wait till done failed with code " << (int) SIRC_P->getLastError();
							// error(tempStream.str());
						// }
					// }	
					
					 // cout<<"wait finished"<<endl; 
					
					//////////// Read the data back
					// if(!SIRC_P->sendRead(0, 8*(numOps-64)+8, (uint8_t *)outputValues)){                     //***********************
						// tempStream << "Read from FPGA failed with code " << (int) SIRC_P->getLastError();
						// error(tempStream.str());
					// }
	
					
					// cout<<"Read finished"<<endl;
					
					
					
					QueryPerformanceCounter((LARGE_INTEGER *) &ctr2);
					QueryPerformanceFrequency((LARGE_INTEGER *) &freq);
					realfpgatime += ((ctr2 - ctr1) * 1.0 / freq);
					
					
					
				// Now accumulate into matrix C
				for(int r=0; r<(N/64); r++)
				{
				  for(int m=0;m<64;m++)
				  {
					matC[i*64+m][r] ^= outputValues[m+32*r] ;
				  }
				} 
				  countervalues[s++] = outputValues[1024];
				 
				// end of computation of matrix block C[i][j]

			}
	}



	
// Print out the C Matrix
	cout << "Matrix C is \n\n" ; 
	   for(int i=0;i<N;i++)
	    {
			for(int j=0;j<N/64;j++)
			{
				// initialize matrix C also    
				cout << matC[i][j]<<"  " ;
			}
			cout << endl;
	    } 
	
	
	cout << "Passed test #3" << endl;
	//cout << "\tExecuted in " << (end - start) << " ms" << endl << endl;
	
	for(int y=0;y< 10;y++)
		cout<<writeTime[y]<<endl;
		
	cout << "Time Taken: "<< ((ctr2 - ctr1) * 1.0 / freq) << std::endl;

	cout << "But fpga time is " << realfpgatime << endl;


	for(int i=0;i<(N/64)*(N/64);i++)
	{
		cout<<dec<<countervalues[i]<< "  ";
	}
	

	


  ofstream myfile;
  myfile.open ("result.txt");
  for(int i=0;i<N;i++)
	    {
			for(int j=0;j<N/64;j++)
			{
				// initialize matrix C also    
				myfile << matC[i][j]<<"  " ;
				
				
			}
			myfile << endl ;
	    } 
  
  myfile << "Time Taken: "<< ((ctr2 - ctr1) * 1.0 / freq) << std::endl;

	myfile << "But fpga time is " << realfpgatime << endl;
  
  myfile.close();


    cin>>a;
	delete SIRC_P;
	delete [] matA;
	delete [] matB;
	delete [] matC;
	free(inputValues);
	free(outputValues);

#ifdef BUGHUNT
    StartLog(susp);
#endif
    PrintZeLog();
	return 0;
}
