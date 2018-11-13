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
Description: SDx Vector Addition using Blocking Pipes Operation
*******************************************************************************/

#define DATA_SIZE 4096
#define INCR_VALUE 10

#include "xcl2.hpp"
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 1)
    {
        std::cout << "Usage: " << argv[0] << std::endl;
        return EXIT_FAILURE;
    }

    //Allocate Memory in Host Memory
    size_t vector_size_bytes = sizeof(int) * DATA_SIZE;

    std::vector<int,aligned_allocator<int>> source_input     (DATA_SIZE);
    std::vector<int,aligned_allocator<int>> source_hw_results(DATA_SIZE);
    std::vector<int,aligned_allocator<int>> source_sw_results(DATA_SIZE);

    // Create the test data and Software Result 
    for(int i = 0 ; i < DATA_SIZE ; i++){
        source_input[i] = i;
        source_sw_results[i] = i + INCR_VALUE;
        source_hw_results[i] = 0;
    }

//OPENCL HOST CODE AREA START
    //Create Program and Kernels.
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue q(context, device,
            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 

    std::string binaryFile = xcl::find_binary_file(device_name,"adder");
    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    cl::Program program(context, devices, bins);
    cl::Kernel krnl_adder_stage(program,"krnl_adder_stage_rtl");
    cl::Kernel krnl_input_stage(program,"krnl_input_stage_rtl");
    cl::Kernel krnl_output_stage(program,"krnl_output_stage_rtl");
    
    //Allocate Buffer in Global Memory
    std::vector<cl::Memory> inBufVec, outBufVec;
    cl::Buffer buffer_input (context,CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, 
            vector_size_bytes,source_input.data());
    cl::Buffer buffer_output(context,CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, 
            vector_size_bytes,source_hw_results.data());
    inBufVec.push_back(buffer_input);
    outBufVec.push_back(buffer_output);

    //Copy input data to device global memory
    cl::Event write_event;
    q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/,NULL,&write_event);

    int inc = INCR_VALUE;
    int size = DATA_SIZE;
    //Set the Kernel Arguments
    krnl_input_stage.setArg(0,buffer_input);
    krnl_input_stage.setArg(1,size);
    krnl_adder_stage.setArg(0,inc);
    krnl_adder_stage.setArg(1,size);
    krnl_output_stage.setArg(0,buffer_output);
    krnl_output_stage.setArg(1,size);

    //Launch the Kernel
    std::vector<cl::Event> eventVec;
    eventVec.push_back(write_event);
    q.enqueueTask(krnl_input_stage, &eventVec);
    q.enqueueTask(krnl_adder_stage, &eventVec);
    q.enqueueTask(krnl_output_stage, &eventVec);

    //wait for all kernels to finish their operations
    q.finish();

    //Copy Result from Device Global Memory to Host Local Memory
    q.enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();

//OPENCL HOST CODE AREA END
    
    // Compare the results of the Device to the simulation
    int match = 0;
    for (int i = 0 ; i < DATA_SIZE ; i++){
        if (source_hw_results[i] != source_sw_results[i]){
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                << " Device result = " << source_hw_results[i] << std::endl;
            match = 1;
            break;
        }
    }

    std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl; 
    return (match ? EXIT_FAILURE :  EXIT_SUCCESS);
}
