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

// Edge Detection Example
#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <sys/time.h>
// OpenCV includes
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <CL/cl.h>
// XCL Helper Library
//#include "xcl2.hpp"

#include "edge.h"

double timestamp(){
double ms=0.0;
timeval time;
gettimeofday(&time,NULL);
ms=(time.tv_sec*1000.0)+(time.tv_usec/1000.0);
return ms;


}

struct Mat{
int rows;
int cols;
int type;
}mat;

 CV_EXPORTS_W void cvtColor( Mat, Mat, int,int   );

short getAbsMax(Mat mat) {
	short max = 0;

	size_t rows = mat.rows;
	size_t cols = mat.cols;

	for(size_t r = 0; r < rows; r++) {
		for(size_t c = 0; c < cols; c++) {
			/*uchar tmp = std::abs(mat.at<uchar>(r,c));
			if(tmp > max) {
				max = tmp;
			}*/
		}
	}

	return max;
}

int main(int argc, char* argv[]) {
	cl_int err;
	//cl::Event event;
    if(argc != 2) {
        std::cout << "Usage: " << argv[0] << "<input>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string inputFileName(argv[1]);
    //cv::Mat inputColor = cv::imread(inputFileName);
     FILE *fp;
    fp=fopen(argv[1],"wb");
    Mat inputColor;
    fread(&inputColor,sizeof(inputFileName),1,fp);
    // Convert image to grayscale then convert to unsigned 8 bit values
    Mat inputRaw, input,output;
   // cv::cvtColor(inputColor, inputRaw, CV_BGR2GRAY);
   // inputRaw.convertTo(input, CV_8UC1);

    inputRaw.rows=input.rows;
    inputRaw.cols=input.cols;
    inputRaw.type=CV_8UC1;

    //Vector IO Buffers are used to avoid unaligned host pointers
    //std::vector<uchar, aligned_allocator<uchar>> image;
    uchar *image;
    image=(uchar *)malloc(sizeof(uchar));

   /* if (input.isContinuous()) {
      image.assign(input.datastart, input.dataend);
    } else {
      for (int i = 0; i < input.rows; ++i) {
        image.insert(image.end(), input.ptr<uchar>(i), input.ptr<uchar>(i)+input.cols);
      }
    }*/

    size_t img_size = input.rows * input.cols;
    //cv::Mat output(input.rows, input.cols, CV_8UC1);
    output.rows=input.rows;
    output.cols=input.cols;
    output.type=CV_8UC1;
    //std::vector<uchar, aligned_allocator<uchar>> outimage(img_size);

    uchar * outimage;
    outimage=(uchar *)malloc(sizeof(img_size));

    std::cout << "Calculating Max Energy..." << std::endl;
    short inputMax = getAbsMax(input);
    std::cout << "inputBits = " << ceil(log2(inputMax)) << " coefMax = 2" << std::endl;
    long long max_bits = (long long) inputMax * 2 * 3*3;
    std::cout << "Max Energy = " << ceil(log2(max_bits)) + 1 << " Bits" << std::endl;

    //std::cout << "Image Dimensions: " << input.cols << "x" << input.rows << std::endl;

    //assert(input.cols == IMAGE_WIDTH);
    //assert(input.rows <= IMAGE_HEIGHT);

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    //std::vector<cl::Device> devices = xcl::get_xil_devices();
    //cl::Device device = devices[0];

    //----------------------------------------------------
    //STEP 1: Discover and Initialize the platforms
    //---------------------------------------------------

	cl_int status;
	cl_platform_id platforms[100];
	cl_uint platforms_n = 0;
	status=clGetPlatformIDs(100, platforms, &platforms_n);
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



    std::cout << "Creating Context..." <<std::endl;
    //OCL_CHECK(err, cl::Context context (device, NULL, NULL, NULL, &err));
    //OCL_CHECK(err, cl::CommandQueue q (context, device, CL_QUEUE_PROFILING_ENABLE, &err));
    //OCL_CHECK(err, std::string device_name = device.getInfo<CL_DEVICE_NAME>(&err));


        cl_device_id devices[100];
	cl_uint numDevices = 0;
	// CL_CHECK(clGetDeviceIDs(NULL, CL_DEVICE_TYPE_ALL, 100, devices, &numDevices));
	status=clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 100, devices, &numDevices);

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




       //-------------------------------------------------------
	//STEP 3 Creating Context
	//-------------------------------------------------------

	cl_context context=NULL;
	context=clCreateContext(NULL,numDevices,devices,NULL,NULL,&status);
	
	//-------------------------------------------------------
	//STEP 4 Creating command queue
	//-------------------------------------------------------
        double first_stamp=timestamp();
	cl_command_queue cmdQueue;
	cmdQueue=clCreateCommandQueue(context,devices[0],0,&status);




    // find_binary_file() is a utility API which will search the xclbin file for
    // targeted mode (sw_emu/hw_emu/hw) and for targeted platforms.
    // std::string binaryFile = xcl::find_binary_file(device_name, "krnl_edge");


       //--------------------------------------------------------------
       //STEP 7 Cerate and compile  program
       //--------------------------------------------------------------

	cl_program program=clCreateProgramWithSource(context,1,(const char**)&input,NULL,&status);
	status=clBuildProgram(program,numDevices,devices,NULL,NULL,NULL);

    //---------------------------------------------------------------------
    //STEP 8 create the kernel
    //------------------------------------------------------------------
	cl_kernel kernel=NULL;
	kernel=clCreateKernel(program,"krnl_sobel",&status);


    // import_binary_file() is a utility API which will load the binaryFile
    // and will return Binaries.
    //cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    //devices.resize(1);
   // OCL_CHECK(err, cl::Program program (context, devices, bins, NULL, &err));
    //OCL_CHECK(err, cl::Kernel krnl_sobel(program, "krnl_sobel", &err));

    // Allocate Buffer in Global Memory
    // Buffers are allocated using CL_MEM_USE_HOST_PTR for efficient memory and
    // Device-to-host communication
    std::cout << "Creating Buffers..." << std::endl;
    cl_mem devInput;
    cl_mem devOutput;
     
    devInput=clCreateBuffer(context,CL_MEM_READ_ONLY,sizeof(cl_uint16),NULL,&status);
    devOutput=clCreateBuffer(context,CL_MEM_WRITE_ONLY,sizeof(cl_uint16),NULL,&status);
   
    // OCL_CHECK(err, cl::Buffer devInput(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
    //		((img_size-1)/32 + 1)*sizeof(cl_uint16), image.data() , &err));
   // OCL_CHECK(err, cl::Buffer devOutput(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
    //		((img_size-1)/32 + 1)*sizeof(cl_uint16), outimage.data(), &err));

    // Copy input data to device global memory
    std::cout << "Copying Buffers to device..." <<std::endl;
   
    //----------------------------------------------------------------------
    //STEP 9 Set the kernel arguements
    //----------------------------------------------------------------------
    status=clSetKernelArg(kernel,0,sizeof(cl_mem),&devInput);
    status|=clSetKernelArg(kernel,1,sizeof(cl_mem),&devOutput);

    // OCL_CHECK(err, err = q.enqueueMigrateMemObjects({devInput}, 0/*0 means from host*/));
  
    
    std::cout << "Setting Arguments..." << std::endl;
    // OCL_CHECK(err, err = krnl_sobel.setArg(0, devInput));
    // OCL_CHECK(err, err = krnl_sobel.setArg(1, devOutput));

    // Launch the Kernel
    // For HLS kernels global and local size is always (1,1,1). So, it is recommended
    // to always use enqueueTask() for invoking HLS kernel
    std::cout << "Launching Kernel..." <<std::endl;
    // OCL_CHECK(err, err = q.enqueueTask(krnl_sobel, NULL, &event));

    //---------------------------------------------------------
	//STEP 10 COnfiguring the work structure
	//---------------------------------------
	size_t globalWorkSize[1];
	globalWorkSize[0]=1;

	//-------------------------------------------------
	//STEP 11 Enqueu the kernel for execution
	//---------------------------------------------------
       cl_event event;
        printf("\nKernel has been launched\n");
	status=clEnqueueNDRangeKernel(cmdQueue,kernel,1,NULL,globalWorkSize,NULL,0,NULL,NULL);
       printf("\nNDRange has been launched\n");

     //--------------------------------------------------
	//STEP 12 READ the kernel 
	//----------------------------------------------------
	clEnqueueReadBuffer(cmdQueue,devOutput,CL_TRUE,0,sizeof(size_t),(void*)"output.bmp",1,NULL,&event);

       printf("\nBuffer is read successfully\n");

    // Copy Result from Device Global Memory to Host Local Memory
    std::cout<< "Getting Result..." << std::endl;
    double second_stamp=timestamp();
  double time_elapsed=second_stamp-first_stamp;
  printf("\nTotal time elspased is %f \n",time_elapsed);
    //OCL_CHECK(err, err = q.enqueueMigrateMemObjects({devOutput}, CL_MIGRATE_MEM_OBJECT_HOST));
   // q.finish();
   // uint64_t nstimestart, nstimeend;
    //OCL_CHECK(err, err = event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_START,&nstimestart));
    //OCL_CHECK(err, err = event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_END,&nstimeend));

   // auto duration = nstimeend-nstimestart;

   // std::cout << "Kernel Duration: " << duration << " ns" <<std::endl;
//OPENCL HOST CODE AREA ENDS

    /*std::*/memcpy(&output, &outimage, sizeof(outimage));

    std::cout << "Calculating Output energy...." << std::endl;
    short outputMax  = getAbsMax(output);

    std::cout << "outputBits = " << ceil(log2(outputMax)) << std::endl;

//    FILE *fp;
    fp=fopen("input.bmp","wb");
    fwrite(&input,1,sizeof(input),fp);
    fclose(fp);
    fp=fopen("output.bmp","wb");
    fwrite(&output,1,sizeof(output),fp);
    fclose(fp);
    


    // cv::imwrite("input.bmp", input);
    // cv::imwrite("output.bmp", output);

    std::cout << "Completed Successfully" << std::endl;

    return EXIT_SUCCESS;
}



void cvtColor(Mat inpuImage1,Mat inputImage,int dist,int src)
{
Mat inpuImage;
Mat inputImage1;
int src1;

inputImage=inputImage1;
inputImage1=inputImage;
src1=dist;

}

