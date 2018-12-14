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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <iostream>
#include <CL/cl.h>
#include <vector>
#include "sys/time.h"
//#include "xcl2.hpp"
//home/atul1/Desktop/SDAccel_examples/libs/bitmap

#include "/home/atul1/Desktop/SDAccel_examples/libs/bitmap/bitmap.h"
#include "/home/atul1/Desktop/SDAccel_examples/libs/bitmap/bitmap.cpp"


double timestamp(){
double ms=0.0;
timeval time;
gettimeofday(&time,NULL);
ms=(time.tv_sec*1000.0)+(time.tv_usec/1000.0);
return ms;


}





void checkErrorStatus(cl_int error, const char* message)
{
  if (error != CL_SUCCESS)
  {
    printf("%s\n", message) ;
    exit(1) ;
  }
}

int main(int argc, char* argv[])
{
  if (argc < 3 || argc > 4 )
  {
    printf("Usage: %s <xclbin> <input bitmap> <golden bitmap(optional)>\n", argv[0]) ;
    return -1 ;
  }
  
  //--------------------------------------------------------------
  //TEST ARGUEMENTS
  //-------------------------------------------------------------
   
  cl_uint num_events_in_wait=1;
  const cl_event *event_wait_list;



  
  // Read the input bit map file into memory
  std::cout << "Reading input image...\n";
  const char* bitmapFilename = argv[2] ;
  const char* goldenFilename;
  int width = 128 ; // Default size
  int height = 128 ; // Default size
 
   BitmapInterface image(bitmapFilename) ;
   bool result = image.readBitmapFile() ;
  /*if (!result)
  {
    return EXIT_FAILURE ;
  }*/
  int *inputImage;
  int *outImage;

  inputImage=(int *)malloc(image.numPixels()*sizeof(int));
  outImage=(int *)malloc(image.numPixels()*sizeof(int));

   //std::vector<int,aligned_allocator<int>> inputImage(image.numPixels() * sizeof(int));
  // std::vector<int,aligned_allocator<int>> outImage(image.numPixels() * sizeof(int));
  if((sizeof(inputImage)==0) || (sizeof(outImage)==0))
  {
    fprintf(stderr, "Unable to allocate the host memory!\n") ;
    return 0;
  }

  width = image.getWidth() ;
  height = image.getHeight() ;

  // Copy image host buffer
  memcpy(inputImage, image.bitmap(), image.numPixels() * sizeof(int));

  // Set up OpenCL hardware and software constructs
  std::cout << "Setting up OpenCL hardware and software...\n";
  cl_int err = 0 ;
  //------------------------------------------------------------------
  // STEP 1 Discover and Initialize the platforms
  //------------------------------------------------------------------
   cl_int status;
   cl_platform_id platforms[100];                  
   cl_uint platforms_n=0;
   status=clGetPlatformIDs(100,platforms,&platforms_n);


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

  // The get_xil_devices will return vector of Xilinx Devices
  // std::vector<cl::Device> devices = xcl::get_xil_devices();
  //cl::Device device = devices[0];

  //----------------------------------------------------------------
  // STEP 2 Discover and Initialize the devices
  //----------------------------------------------------------------
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



  //---------------------------------------------------------------
  // STEP 3 Creating Context
  //---------------------------------------------------------------
   cl_context context=NULL;
   context=clCreateContext(NULL,numDevices,devices,NULL,NULL,&status);



  // Creating Context and Command Queue for selected Device
  // cl::Context context(device);
  //cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
  //std::string device_name = device.getInfo<CL_DEVICE_NAME>();
//  std::cout << "Found Device=" << device_name.c_str() << std::endl;

  //-------------------------------------------------------------------
  // STEP 4 Creating command queue
  //-------------------------------------------------------------------
  cl_command_queue cmdQueue;
  cmdQueue=clCreateCommandQueue(context,devices[0],0,&status);




  // import_binary() command will find the OpenCL binary file created using the 
  // xocc compiler load into OpenCL Binary and return as Binaries
  /*std::string binaryFile = xcl::find_binary_file(device_name,"krnl_median");
  cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
  devices.resize(1);
  cl::Program program(context, devices, bins);*/

  // These commands will allocate memory on the FPGA. The cl::Buffer objects can
  // be used to reference the memory locations on the device.
  

  cl_mem buffer_input;
  cl_mem buffer_output;

  buffer_input=clCreateBuffer(context,CL_MEM_READ_ONLY,image.numPixels()*sizeof(int),NULL,&status);
 buffer_output=clCreateBuffer(context,CL_MEM_WRITE_ONLY,image.numPixels()*sizeof(int),NULL,&status);

  //cl::Buffer buffer_input(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, image.numPixels() * sizeof(int), inputImage.data());
  //cl::Buffer buffer_output(context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, image.numPixels() * sizeof(int), outImage.data());

  //cl::Event kernel_Event, read_Event, write_Event;

  // These commands will load the input_image and output_image vectors from the host
  // application into the buffer_input and buffer_output cl::Buffer objects.
  std::cout << "Writing input image to buffer...\n";
  //err = q.enqueueMigrateMemObjects({buffer_input},0/* 0 means from host*/, NULL, &write_Event);

  //-----------------------------------------------------------------------------------------
  // STEP 7 Create and compile program
  //-----------------------------------------------------------------------------------------

  cl_program program=clCreateProgramWithSource(context,1,(const char**)&inputImage,NULL,&status);
  status=clBuildProgram(program,numDevices,devices,NULL,NULL,NULL);





  checkErrorStatus(err, "Unable to enqueue write buffer") ;

   double first_stamp=timestamp();

  //-----------------------------------------------------------------------------------------------
  //    STEP 8 create kernel
  //---------------------------------------------------------------------------------------
 cl_kernel kernel=NULL;
 kernel=clCreateKernel(program,"krnl_medianFilter",&status);


  // This call will extract a kernel out of the program we loaded in the previous line
//  cl::Kernel krnl_medianFilter(program, "median");

  // set the kernel Arguments
  std::cout << "Setting arguments and enqueueing kernel...\n";
  //krnl_medianFilter.setArg(0, buffer_input);
  //krnl_medianFilter.setArg(1, buffer_output);
  //krnl_medianFilter.setArg(2, width);
  //krnl_medianFilter.setArg(3, height);

  //------------------------------------------------------------------
  // STEP 9 Setting the kernel Arguements
  //------------------------------------------------------------------
  status=clSetKernelArg(kernel,0,sizeof(cl_mem),&buffer_input);
  status |=clSetKernelArg(kernel,1,sizeof(cl_mem),&buffer_output);
  status=clSetKernelArg(kernel,2,sizeof(cl_mem),&width);
  status=clSetKernelArg(kernel,3,sizeof(cl_mem),&height);

  std::cout << "Arguements are set ....\n";

  //std::vector<cl::Event> eventList;
  //eventList.push_back(write_Event);

  //-----------------------------------------------------------------
  // STEP 10 Configuring the work structure
  //----------------------------------------------------------------
  
  size_t globalWorkSize[1];
  globalWorkSize[0]=1;
  std::cout << "work structure configured ....\n";
  //----------------------------------------------------------------
  // STEP 11 Enqueue The kernel for execution
  //--------------------------------------------------------------
  status=clEnqueueNDRangeKernel(cmdQueue,kernel,1,NULL,globalWorkSize,NULL,0,NULL,NULL);
  std::cout << "kernel enqueued .....\n";
  // This function will execute the kernel on the FPGA
  //err = q.enqueueTask(krnl_medianFilter, &eventList, &kernel_Event);  
  //checkErrorStatus(err, "Unable to enqueue Task") ;

  //-------------------------------------------------------------------
  // STEP 12 READ the kernel
  //------------------------------------------------------------------
   clEnqueueReadBuffer(cmdQueue,buffer_output,CL_TRUE,0,sizeof(size_t),(void *)"output.bmp",num_events_in_wait,NULL,NULL);
   std::cout << "Reading the kernel ....\n";
  // Read back the image from the kernel
  std::cout << "Reading output image and writing to file...\n";

  double second_stamp=timestamp();
  double time_elapsed=second_stamp-first_stamp;
  printf("\nTotal time elspased is %f \n",time_elapsed);

  //eventList.clear();
  //eventList.push_back(kernel_Event);
  //err = q.enqueueMigrateMemObjects({buffer_output}, CL_MIGRATE_MEM_OBJECT_HOST, &eventList, &read_Event);
  //checkErrorStatus(err, "Unable to enqueue read buffer") ;

  //q.flush();
  //q.finish();

 bool match = true;
  if (argc == 4){
      goldenFilename = argv[3];
      //Read the golden bit map file into memory
     BitmapInterface goldenImage(goldenFilename);
      result = goldenImage.readBitmapFile() ;
      if (!result)
      {
          std::cout << "ERROR:Unable to Read Golden Bitmap File "<< goldenFilename << std::endl;
          return EXIT_FAILURE ;
      }
      //Compare Golden Image with Output image
      if ( image.getHeight() != goldenImage.getHeight() || image.getWidth() != goldenImage.getWidth()){
          match = false;
      }else{
          int* goldImgPtr = goldenImage.bitmap();
          for (unsigned int i = 0 ; i < image.numPixels(); i++){
              if (outImage[i] != goldImgPtr[i]){
                  match = false;
                  printf ("Pixel %d Mismatch Output %x and Expected %x \n", i, outImage[i], goldImgPtr[i]);
                  break;
              }
          }
      }
  }

  // Write the final image to disk
  printf("Writing RAW Image \n");
  image.writeBitmapFile(outImage);

  std::cout << (match ? "TEST PASSED" : "TEST FAILED") << std::endl;
  return (match ? EXIT_SUCCESS : EXIT_FAILURE) ;

}
