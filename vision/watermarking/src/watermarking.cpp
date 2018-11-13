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
#include <iostream>
#include <vector>
//Includes
#include "xcl2.hpp"
#include "bitmap.h"

int main(int argc, char* argv[])
{
    cl_int err;
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <input bitmap> <golden bitmap>" << std::endl;
        return EXIT_FAILURE ;
    }
    const char* bitmapFilename = argv[1];
    const char* goldenFilename;

    //Read the input bit map file into memory
    BitmapInterface image(bitmapFilename);
    bool result = image.readBitmapFile() ;
    if (!result)
    {
        std::cout << "ERROR:Unable to Read Input Bitmap File "<< bitmapFilename << std::endl;
        return EXIT_FAILURE ;
    }

    int width = image.getWidth() ;
    int height = image.getHeight();

    //Allocate Memory in Host Memory
    size_t image_size = image.numPixels();
    size_t image_size_bytes = image_size * sizeof(int);
    std::vector<int,aligned_allocator<int>> inputImage(image_size);
    std::vector<int,aligned_allocator<int>> outImage(image_size);

    // Copy image host buffer
    memcpy(inputImage.data(), image.bitmap(), image_size_bytes);

// OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    std::cout << "Creating Context..." << std::endl;
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    OCL_CHECK(err, cl::Context context (device, NULL, NULL, NULL, &err));
    OCL_CHECK(err, cl::CommandQueue q (context, device, CL_QUEUE_PROFILING_ENABLE, &err));
    OCL_CHECK(err, std::string device_name = device.getInfo<CL_DEVICE_NAME>(&err));

    // find_binary_file() is a utility API which will search the xclbin file for
    // targeted mode (sw_emu/hw_emu/hw) and for targeted platforms.
    std::string binaryFile = xcl::find_binary_file(device_name, "apply_watermark");

    // import_binary_file() is a utility API which will load the binaryFile
    // and will return Binaries.
    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    OCL_CHECK(err, cl::Program program (context, devices, bins, NULL, &err));
    OCL_CHECK(err, cl::Kernel apply_watermark(program, "apply_watermark", &err));

    // For Allocating Buffer to specific Global Memory Bank, user has to use cl_mem_ext_ptr_t
    // and provide the Banks
    cl_mem_ext_ptr_t inExt={0}, outExt={0};  // Declaring two extensions for both buffers
    inExt.flags  = XCL_MEM_DDR_BANK0; // Specify Bank0 Memory for input memory
    outExt.flags = XCL_MEM_DDR_BANK1; // Specify Bank1 Memory for output Memory
    inExt.obj 	 = inputImage.data();
    outExt.obj   = outImage.data(); // Setting Param to Zero
    inExt.param  = 0 ; outExt.param = 0;

    // Allocate Buffer in Global Memory
    // Buffers are allocated using CL_MEM_USE_HOST_PTR for efficient memory and
    // Device-to-host communication
    std::cout << "Creating Buffers..." << std::endl;
    OCL_CHECK(err, cl::Buffer buffer_inImage(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
    		image_size_bytes, &inExt, &err));
    OCL_CHECK(err, cl::Buffer buffer_outImage(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
    		image_size_bytes, &outExt, &err));

    // Copy input data to device global memory
    std::cout<< "Copying data..." << std::endl;
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_inImage}, 0/*0 means from host*/));

    std::cout<< "Setting arguments..." <<std::endl;
    OCL_CHECK(err, err = apply_watermark.setArg(0, buffer_inImage));
    OCL_CHECK(err, err = apply_watermark.setArg(1, buffer_outImage));
    OCL_CHECK(err, err = apply_watermark.setArg(2, width));
    OCL_CHECK(err, err = apply_watermark.setArg(3, height));

    // Launch the Kernel
    // For HLS kernels global and local size is always (1,1,1). So, it is recommended
    // to always use enqueueTask() for invoking HLS kernel
    std::cout <<"Launching Kernel... " << std::endl;
    OCL_CHECK(err, err = q.enqueueTask(apply_watermark));

    // Copy Result from Device Global Memory to Host Local Memory
    std::cout << "Getting Results..." << std::endl;
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_outImage}, CL_MIGRATE_MEM_OBJECT_HOST));
    q.finish();
//OPENCL HOST CODE AREA ENDS

    bool match = true;
    if (argc > 2){
        goldenFilename = argv[2];
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
    image.writeBitmapFile(outImage.data());

    std::cout << (match ? "TEST PASSED" : "TEST FAILED") << std::endl;
    return (match ? EXIT_SUCCESS : EXIT_FAILURE) ;
}
