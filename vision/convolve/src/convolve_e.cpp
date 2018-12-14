/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/
// Convolve Example
#define FILTER_WIDTH 11
#define FILTER_HEIGHT 11

#define IMAGE_WIDTH 1024
#define IMAGE_HEIGHT 1024

#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <CL/cl.h>
#include "sys/time.h"
#include "/usr/include/opencv2/opencv.hpp"
//#include "/usr/include/opencv2/core/mat.hpp"
#include "/usr/include/opencv2/imgproc/imgproc.hpp"

// OpenCV includes

double timestamp(){
double ms=0.0;
timeval time;
gettimeofday(&time,NULL);
ms=(time.tv_sec*1000.0)+(time.tv_usec/1000.0);
return ms;


}

struct Mat {
char *name;
int rows;
int cols;
int type;



}mat;

short getAbsMax( /*cv::*/Mat mat, size_t rows, size_t cols) {
	short max = 0;

	for(size_t r = 0; r < rows; r++) {
		for(size_t c = 0; c < cols; c++) {
			/*short tmp = std::abs(mat.at<short>(r,c));
			if(tmp > max) {
				max = tmp;
			}*/
		}
	}

	return max;
}

/*cv::*/Mat readTxtFile(std::string fileName, size_t rows, size_t cols) {

    //cv::Mat mat(rows, cols, CV_16S);
 	mat.rows=rows;
	mat.cols=cols;
	mat.type=CV_16S;

    std::ifstream txtFile(fileName.c_str());

    if(!txtFile.is_open()) {
        std::cout << "ERROR: Could not open file " << fileName << std::endl;
        abort();
    }

    for(size_t r = 0; r < rows; r++) {
        for(size_t c = 0; c < cols; c++) {
            //txtFile >> mat.at<short>(r,c);
        }
    }

   return mat;
      
}

/*cv::*/Mat readFloatTxtFile(std::string fileName, size_t rows, size_t cols) {

   // cv::Mat mat(rows, cols, CV_32F);
   mat.rows=rows;
   mat.cols=cols;
   mat.type=CV_32F;
    std::ifstream txtFile(fileName.c_str());

    if(!txtFile.is_open()) {
        std::cout << "ERROR: Could not open file " << fileName << std::endl;
        abort();
    }

    for(size_t r = 0; r < rows; r++) {
        for(size_t c = 0; c < cols; c++) {
          // txtFile >> mat.at<float>(r,c);
        }
    }

    return mat;
    //return 0;
}

int main(int argc, char* argv[]) {
	cl_int err;
	//cl::Event event;

    if(argc != 3 && argc != 4)
    {
        std::cout << "Usage: " << argv[0] <<"<input> <coef> [<golden>]" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Parsing Command Line..." << std::endl;
    std::string inputFileName(argv[1]);
    std::string coefFileName(argv[2]);

    bool validate = false;

    if (argc == 4) {
        validate = true;
    }

    std::cout << "Reading inputs..." << std::endl;
    /*cv::*///Mat input  = readTxtFile(inputFileName, IMAGE_HEIGHT, IMAGE_WIDTH);
   /*cv::Mat*/Mat coef   = readTxtFile(coefFileName, FILTER_HEIGHT, FILTER_WIDTH);
    Mat input;
    //input->name=inputFileName;
    input.rows=IMAGE_HEIGHT;
    input.cols=IMAGE_WIDTH;
    //input /= 32;
    //coef *= 1;

   // std::vector<double, aligned_allocator<double>> vecInput;
    double *vecInput;
    double *vecOutput;
    vecInput=(double *)malloc(1048576*sizeof(double));      //how much meory should be allocated to this?

    //vecInput=assign((double *)input.datastart,(double *)input.dataend);
     

 // std::vector<double, aligned_allocator<double>> vecOutput(vecInput.size());
    vecOutput=(double *)malloc(1048576*sizeof(double));
   

    std::cout << "Calculating Max Energy..." << std::endl;
    short inputMax = getAbsMax(input, IMAGE_HEIGHT, IMAGE_WIDTH);
    short coefMax  = getAbsMax(coef, FILTER_HEIGHT, FILTER_WIDTH);

    std::cout << "inputBits = " << ceil(log2(inputMax)) << " coefMax = " << ceil(log2(coefMax)) << std::endl;
    long long max_bits = (long long) inputMax * coefMax * FILTER_HEIGHT * FILTER_WIDTH;
   // cv::Mat output(IMAGE_HEIGHT, IMAGE_WIDTH, CV_16S);
    Mat output;
    output.rows=IMAGE_HEIGHT;
    output.cols=IMAGE_WIDTH;
    output.type=CV_16S;
    //output.reshape(0);

    std::cout << "Max Energy = " << ceil(log2(max_bits)) + 1 << " Bits" << std::endl;

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the Xilinx
    // platforms and will return list of devices connected to Xilinx platform
    //std::vector<cl::Device> devices = xcl::get_xil_devices();
    
    //-------------------------------------------------------------------------------------
    //STEP 1: Discover and Initialize the paltforms
    //-------------------------------------------------------------------------------------
    cl_int status;
    cl_platform_id platforms[100];
    cl_uint platforms_n=0;
    status=clGetPlatformIDs(100,platforms,&platforms_n);   //what should be number here instead of 100?

    printf("=== %d OpenCL platform(s) found: ===\n", platforms_n);
	for (int i=0; i<platforms_n; i++)
	{
		char buffer[10240];
		printf("  -- %d --\n", i);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 10240, buffer, NULL);
		//printf("  PROFILE = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 10240, buffer, NULL);
		printf("  VERSION = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 10240, buffer, NULL);
		printf("  NAME = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 10240, buffer, NULL);
		printf("  VENDOR = %s\n", buffer);
		status=clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, 10240, buffer, NULL);
		//printf("  EXTENSIONS = %s\n", buffer);
	}

	if (platforms_n == 0)
	{	
		printf("No OpenCL platform found!\n\n");
		//return 1;
	}


    //cl::Device device = devices[0];

    //-----------------------------------------------------------------------------------------
    //STEP 2: Discover and Initialize the devices
    //-----------------------------------------------------------------------------------------
    cl_device_id devices[100];
    cl_uint numDevices=0;
    status=clGetDeviceIDs(platforms[0],CL_DEVICE_TYPE_GPU,100,devices,&numDevices);



    printf("=== %d OpenCL device(s) found on platform:\n", platforms_n);
	for (int i=0; i<numDevices; i++)
	{
		char buffer[10240];
		cl_uint buf_uint;
		cl_ulong buf_ulong;
		printf("  -- %d --\n", i);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
		printf("  DEVICE_NAME = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_VENDOR, sizeof(buffer), buffer, NULL);
		printf("  DEVICE_VENDOR = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_VERSION, sizeof(buffer), buffer, NULL);
		//printf("  DEVICE_VERSION = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DRIVER_VERSION, sizeof(buffer), buffer, NULL);
		//printf("  DRIVER_VERSION = %s\n", buffer);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(buf_uint), &buf_uint, NULL);
		printf("  DEVICE_MAX_COMPUTE_UNITS = %u\n", (unsigned int)buf_uint);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(buf_uint), &buf_uint, NULL);
		//printf("  DEVICE_MAX_CLOCK_FREQUENCY = %u\n", (unsigned int)buf_uint);
		status=clGetDeviceInfo(devices[i], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(buf_ulong), &buf_ulong, NULL);
		//printf("  DEVICE_GLOBAL_MEM_SIZE = %llu\n", (unsigned long long)buf_ulong);
		printf("\n");
	}

	if (numDevices == 0)
	{	printf("No OpenCL device found!\n\n");
		//return 1;
	}

    //-----------------------------------------------------------------------------------------
    // STEP 3: Creating context
    //-----------------------------------------------------------------------------------------

    cl_context context=NULL;
    context=clCreateContext(NULL,numDevices,devices,NULL,NULL,&status);


    std::cout << "Creating Context..." <<std::endl;
    //OCL_CHECK(err, cl::Context context (device, NULL, NULL, NULL, &err));
    //OCL_CHECK(err, cl::CommandQueue q (context, device, CL_QUEUE_PROFILING_ENABLE, &err));
    //OCL_CHECK(err, std::string device_name = device.getInfo<CL_DEVICE_NAME>(&err));

    //--------------------------------------------------------------------------------------------
    // STEP 4 Creating Command Queue
    //--------------------------------------------------------------------------------------------
    double first_stamp=timestamp(); 
    cl_command_queue cmdQueue;
    cmdQueue=clCreateCommandQueue(context,devices[0],0,&status);

    cl_mem devCoef;
    cl_mem devInput;
    cl_mem devOutput;

    devCoef=clCreateBuffer(context,CL_MEM_READ_ONLY,((FILTER_HEIGHT*FILTER_WIDTH-1)/32+1)*sizeof(cl_uint16),NULL,&status);
    devInput=clCreateBuffer(context,CL_MEM_READ_ONLY,((IMAGE_HEIGHT*IMAGE_WIDTH-1)/32+1)*sizeof(cl_uint16),NULL,&status);
    devOutput=clCreateBuffer(context,CL_MEM_WRITE_ONLY,((IMAGE_HEIGHT*IMAGE_WIDTH-1)/32+1)*sizeof(cl_uint16),NULL,&status);



    std::cout << "Creating Buffers..." <<std::endl;
    
    //------------------------------------------------------------------------------------
    //STEP 7 Create and compile program
    //------------------------------------------------------------------------------------
    cl_program program=clCreateProgramWithSource(context,1,(const char**)&input,NULL,&status);
    
    status=clBuildProgram(program,numDevices,devices,NULL,NULL,NULL);
    printf("\nCreating the program");
    //------------------------------------------------------------------------------------
    // STEP 8 Create th kernel
    //------------------------------------------------------------------------------------
     
    cl_kernel kernel=NULL;
    kernel=clCreateKernel(program,"krnl_convolve",&status);
    printf("\nCreating the kernel");
    // Copy input data to device global memory
    std::cout << "Copying data..." << std::endl;
    //OCL_CHECK(err, err = q.enqueueMigrateMemObjects({devCoef, devInput}, 0/*0 means from host*/));

    //------------------------------------------------------------------------------------
    // STEP 9 Setting up thekernel arguements
    //------------------------------------------------------------------------------------
    status=clSetKernelArg(kernel,0,sizeof(cl_mem),&devCoef);
    status=clSetKernelArg(kernel,0,sizeof(cl_mem),&devInput);
    status=clSetKernelArg(kernel,0,sizeof(cl_mem),&devOutput);


    std::cout << "Setting Arguments..." << std::endl;
    //OCL_CHECK(err, err = krnl_convolve.setArg(0, devCoef));
    //OCL_CHECK(err, err = krnl_convolve.setArg(1, devInput));
    //OCL_CHECK(err, err = krnl_convolve.setArg(2, devOutput));
    
    // Launch the Kernel
    // For HLS kernels global and local size is always (1,1,1). So, it is recommended
    // to always use enqueueTask() for invoking HLS kernel
    

    //-----------------------------------------------------------------------------------
    // STEP 10 Configuring the work structure
    //-----------------------------------------------------------------------------------
    size_t globalWorkSize[1];
    globalWorkSize[0]=1;            //What should be the value here?



    //---------------------------------------------------------------------------------
    // STEP 11 Enqueue the kernel for execution
    //---------------------------------------------------------------------------------
    cl_event event;
    std::cout << "Launching Kernel..." << std::endl;
    //OCL_CHECK(err, err = q.enqueueTask(krnl_convolve, NULL, &event));
    status=clEnqueueNDRangeKernel(cmdQueue,kernel,1,NULL,globalWorkSize,NULL,0,NULL,NULL);


    //----------------------------------------------------------------------------------
    // STEP 12 Read the KErnel
    //----------------------------------------------------------------------------------
     clEnqueueReadBuffer(cmdQueue,devOutput,CL_TRUE,0,sizeof(size_t),(void *)"output.bmp",1,NULL,&event);


    // Copy Result from Device Global Memory to Host Local Memory
    std::cout << "Getting Results..." << std::endl;
    double second_stamp=timestamp();
  double time_elapsed=second_stamp-first_stamp;
  printf("\nTotal time elspased is %f \n",time_elapsed);
    //OCL_CHECK(err, err = q.enqueueMigrateMemObjects({devOutput}, CL_MIGRATE_MEM_OBJECT_HOST));
    //OCL_CHECK(err, err = q.finish());
    uint64_t nstimestart, nstimeend;
    //OCL_CHECK(err, err = event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_START,&nstimestart));
    //OCL_CHECK(err, err = event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_END,&nstimeend));

   // auto duration = nstimeend-nstimestart;

    //std::cout << "Kernel Duration: " << duration << " ns" <<std::endl;
//OPENCL HOST CODE AREA ENDS

    std::memcpy(&output, vecOutput, sizeof(vecOutput)*sizeof(double));

    short outputMax  = getAbsMax(output, IMAGE_HEIGHT, IMAGE_WIDTH);
    std::cout << "outputBits = " << ceil(log2(outputMax)) << std::endl;

    /*cv::imwrite("input.bmp",input);
    cv::imwrite("output.bmp", output);
    cv::imwrite("coef.bmp", coef);*/
  
    FILE *fp;
    fp=fopen("input.bmp","wb");
    fwrite(&input,1,sizeof(input),fp);
    fclose(fp);
    fp=fopen("output.bmp","wb");
    fwrite(&output,1,sizeof(output),fp);
    fclose(fp);
    fp=fopen("coef.bmp","wb");
    fwrite(&coef,1,sizeof(coef),fp);
    fclose(fp);

  /* if(validate) {
        std::cout << "Validate" << std::endl;
        std::string goldenFileName(argv[3]);
        cv::Mat golden  = readFloatTxtFile(goldenFileName, IMAGE_HEIGHT, IMAGE_WIDTH);

        cv::imwrite("golden.bmp", golden);
    }*/

    std::cout << "Completed Successfully" << std::endl;
    return EXIT_SUCCESS;
}


