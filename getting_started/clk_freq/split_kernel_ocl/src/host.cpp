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

/*******************************************************************************
Description: 

        This is a image processing application to show case effectiveness of split
        kernel approach instead of single mega kernel. This implementation is a
        single complex kernel.

*******************************************************************************/
#include "xcl2.hpp"
#include <vector>

// Bitmap image utility
#include "bitmap.h"

// Support functions
#define SW_FLOW
#include "defns.h"

// Software solution of image sketch operation
void software_sketch(int *image, int *output, int width, int height)
{
    printf("Software Solution Launched.... \n");
    int temp_res[MAX_WIDTH * MAX_HEIGHT]; //Hold boost and sketch outputs
    int med_out[MAX_WIDTH * MAX_HEIGHT];
    
    //Hold the window for processing
    uint rgbWindow[SIZE];
    
    // Holds the rows/lines from which the data is to be picked
    int lineAddr[WINDOW];
    
    
    // Boost Stage
    // Do boost filter on the image and write the output to temp_res
    for(int row = 0; row < height; row++){
        for(int col = 0; col < width; col++){
            
            // Get pixels within 3x3 aperture
            
            lineAddr[0] = row - 1;
            lineAddr[1] = row;
            lineAddr[2] = row + 1;
            
            // Handle Boundary Conditions
            if(row == 0){
                lineAddr[0] = 0;
            }
            else if(row == height-1) {
                lineAddr[0] = row-2;
                lineAddr[1] = row-1;
                lineAddr[2] = row;
            }
            
            // The fillWindow loop is unrolled automatically because it is
            // inlined into a loop that is pipelined.
            for(int i = 0; i < WINDOW; i++){
                rgbWindow[i*WINDOW + 0] = (col == 0)? image[lineAddr[i]*width + col] : image[lineAddr[i]*width + col-1];
                rgbWindow[i*WINDOW + 1] = image[lineAddr[i]*width + col];
                rgbWindow[i*WINDOW + 2] = (col == width-1)? image[lineAddr[i]*width + col] : image[lineAddr[i]*width + col+1];
            }
            
            // Boost Value of the 3x3 rgbWindow
            // getBoost() is defined in kernels/boost_helper.h   
            temp_res[row*width + col] = getBoost(rgbWindow);
        }
    }
    
    // Median Stage
    // Do median filter on the image and write output to med_out
    for(int row = 0; row < height; row++){
        for(int col = 0; col < width; col++){
            
            // Get pixels within 3x3 aperture
            
            lineAddr[0] = row - 1;
            lineAddr[1] = row;
            lineAddr[2] = row + 1;
            
            // Handle Boundary Conditions
            if(row == 0){
                lineAddr[0] = 0;
            }
            else if(row == height-1) {
                lineAddr[0] = row-2;
                lineAddr[1] = row-1;
                lineAddr[2] = row;
            }
            
            // The fillWindow loop is unrolled automatically because it is
            // inlined into a loop that is pipelined.
            for(int i = 0; i < WINDOW; i++){
                rgbWindow[i*WINDOW + 0] = (col == 0)? image[lineAddr[i]*width + col] : image[lineAddr[i]*width + col-1];
                rgbWindow[i*WINDOW + 1] = image[lineAddr[i]*width + col];
                rgbWindow[i*WINDOW + 2] = (col == width-1)? image[lineAddr[i]*width + col] : image[lineAddr[i]*width + col+1];
            }
            
            // Median of the 3x3 rgbWindow
            // getMedian() is defined in kernels/median_helper.h
            med_out[row*width + col] = getMedian(rgbWindow);                     
        }
    }
    
    // Sketch Stage
    // Do sketch operation on boost and median outputs.
    // Boost outputs are present in temp_res
    // Write the result back into temp_res.
    for(int i = 0; i < width*height; i++) {
        // Sketch operation on the current pixel of boost and median outputs
        // Boost operation output is in temp_res and
        // Median operation output is in med_out.
        // getSketch() is defined in kernels/sketch_helper.h
        temp_res[i] = getSketch(temp_res[i], med_out[i]);           

    }
    
    // Flips the Image by Reading Output Results from Sketch Output 
    // Burst write back results onto output
    for(int row = 0 ; row < height ; row++){
        // Reads from temp_res and flip the row and burst write output
        for(int col = 0; col < width; col++){
            output[row*width + col] = temp_res[row*width + width-col-1];
        }
    }
}//end of software

void run_opencl_sketch
 (
   std::vector<cl::Device> &devices,
   cl::CommandQueue &q,
   cl::Context &context,
   std::string &device_name,
   bool good,
   std::vector<int, aligned_allocator<int>> &hw_inImage,
   std::vector<int, aligned_allocator<int>> &hw_outImage,
   int size,
   int width,
   int height
 )
 {
    std::string binaryFile;
    size_t image_size_bytes = sizeof(int) * size;

    if(good)
       binaryFile = xcl::find_binary_file(device_name,"sketch_GOOD");
    else
       binaryFile = xcl::find_binary_file(device_name,"sketch_BAD");


    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    cl::Program program(context, devices, bins);
    cl::Kernel krnl_process_image(program,"process_image");

    //Allocate Buffer in Global Memory
    std::vector<cl::Memory> inBufVec, outBufVec;
    cl::Buffer buffer_input (context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                  image_size_bytes,hw_inImage.data());
    cl::Buffer buffer_output(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
            image_size_bytes,hw_outImage.data());
    inBufVec.push_back(buffer_input);
    outBufVec.push_back(buffer_output);

    std::cout << "Writing input image to buffer...\n";

    //Copy input data to device global memory
    q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
    
    int narg = 0;
    //Set the Kernel Arguments
    krnl_process_image.setArg(narg++, buffer_input);
    krnl_process_image.setArg(narg++, buffer_output);
    krnl_process_image.setArg(narg++, width);
    krnl_process_image.setArg(narg++, height);
    
    std::cout << "Launching Kernels...." << std::endl;

    //Launch the Kernel
    q.enqueueTask(krnl_process_image);
    std::cout << "Kernel Execution Finished...." << std::endl;
    
    //Copy Result from Device Global Memory to Host Local Memory
    q.enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

  }  
 

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <input bitmap>" << std::endl;
        return EXIT_FAILURE ;
    }
    const char* bitmapFilename = argv[1];

    //Read the bitmap file into memory and allocate memory 
    std::cout << "Reading input image...\n";

    BitmapInterface image(bitmapFilename);

    bool result = image.readBitmapFile();
    if(!result) {
        std::cout << "Error reading bitmap file : " << bitmapFilename << std::endl;
        return -1;
    }

    int width  = image.getWidth();
    int height = image.getHeight();
    
    if(width > MAX_WIDTH || height > MAX_HEIGHT) {
        std::cout << "Error file larger than max dimension" << std::endl;
        std::cout << "Current File of size : " << height << "x" << width << std::endl;
        std::cout << "Max Supported size is " << MAX_HEIGHT << "x"<< MAX_WIDTH << std::endl;
        return -1;
    }

    size_t image_size_bytes = sizeof(int) * image.numPixels();
    std::vector<int,aligned_allocator<int>> hw_inImage(image.numPixels());
    std::vector<int,aligned_allocator<int>> hw_outImage(image.numPixels());
    std::vector<int,aligned_allocator<int>> sw_outImage(image.numPixels());
    //Copying image host buffer
    memcpy(hw_inImage.data(),image.bitmap(),image_size_bytes);

    int size = image.numPixels();

//OPENCL HOST CODE AREA START

    //Create Program and Kernels. 
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 

    run_opencl_sketch(devices,q,context,device_name,true,hw_inImage,hw_outImage,size,width,height);
    run_opencl_sketch(devices,q,context,device_name,false,hw_inImage,hw_outImage,size,width,height);

//OPENCL HOST CODE AREA END

    // Software sketch function 
    software_sketch(image.bitmap(), sw_outImage.data(), width, height);

    // Compare software with hardware results
    int match = 0;
    for(int i = 0; i < size; i++){
        if(sw_outImage[i] != hw_outImage[i]){
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " CPU result = " << sw_outImage[i] << " Device result = " << hw_outImage[i] << std::endl;
            match = 1;
            break;
        } 
    }
    //Write the final image to disk
    image.writeBitmapFile(hw_outImage.data());
 
    std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl; 
    return (match ? EXIT_FAILURE :  EXIT_SUCCESS);
}
