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
#include "xcl2.hpp"
#include <vector>

using std::vector;

static const int DATA_SIZE = 256;
static const std::string error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

// This example illustrates the simple OpenCL example that performs
// buffer copy from one buffer to another
int main(int argc, char **argv) {
    size_t size_in_bytes = DATA_SIZE * sizeof(int);

    vector<int,aligned_allocator<int>> source_a(DATA_SIZE, 13);
    vector<int,aligned_allocator<int>> source_results(DATA_SIZE);


    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 
    std::cout << "Found Device=" << device_name.c_str() << std::endl;

    std::string binaryFile = xcl::find_binary_file(device_name,"vector_addition");
    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    cl::Program program(context, devices, bins);

    std::vector<cl::Memory> inBufVec, outBufVec;
    cl::Buffer buffer_a(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,  
            size_in_bytes, source_a.data());
    cl::Buffer buffer_result(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, 
            size_in_bytes,source_results.data());
    cl::Buffer buffer_b(context, CL_MEM_READ_ONLY,  size_in_bytes);
    inBufVec.push_back(buffer_a);
    outBufVec.push_back(buffer_result);

    //copy buffer a to device.
    q.enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);

    // This enqueueCopyBuffer() command will copy buffer from buffer_a to buffer_b
    q.enqueueCopyBuffer(buffer_a, buffer_b,0,0, size_in_bytes);

    cl::Kernel kernel(program,"vector_add");
    auto krnl_vector_add = cl::KernelFunctor<cl::Buffer&, cl::Buffer&, cl::Buffer&, 
         int>(kernel);

    // This function will execute the kernel on the FPGA
    krnl_vector_add(cl::EnqueueArgs(q, cl::NDRange(1,1,1), cl::NDRange(1,1,1)), 
            buffer_result, buffer_a,buffer_b,DATA_SIZE);

    q.enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
    q.finish();


    int match = 0;
    printf("Result = \n");
    for (int i = 0; i < DATA_SIZE; i++) {
        int host_result = source_a[i] + source_a[i];
        if (source_results[i] != host_result) {
            printf(error_message.c_str(), i, host_result, source_results[i]);
            match = 1;
            break;
        } else {
          printf("%d ", source_results[i]);
          if (((i + 1) % 16) == 0) printf("\n");
        }
    }

    std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl; 
    return (match ? EXIT_FAILURE :  EXIT_SUCCESS);
}
