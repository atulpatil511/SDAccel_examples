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

/******************************************************************************
Description:

    This example demonstrates how to create OpenCL subdevices which uses a 
    single kernel multiple times in order to show how to handle each instance 
    independently including independent buffers, command queues and sequencing.

******************************************************************************/
#include "xcl2.hpp"
#include <vector>

//Number of sub devices
#define N 8 

//Array size
#define MB (1024*1024)
#define DATA_SIZE (MB*N)

int main(int argc, char** argv) {

    int size;
    const char *xcl_emu = getenv("XCL_EMULATION_MODE");
    if(xcl_emu && !strcmp(xcl_emu, "hw_emu")) {
        //Original dataset is reduced for faster execution of hardware 
        //emulation flow
        size = 1024*N;
    }
    else{
        size = DATA_SIZE;
    }

    //Allocate memory in Host Memory
    int vector_size_bytes = sizeof(int) * size; 
    size_t elements_per_subdevice = size / N;
    size_t size_per_subdevice = sizeof(int) * elements_per_subdevice;

    std::vector<int,aligned_allocator<int>> h_a(vector_size_bytes);
    std::vector<int,aligned_allocator<int>> h_b(vector_size_bytes);
    std::vector<int,aligned_allocator<int>> hw_results(vector_size_bytes);
    std::vector<int> sw_results(vector_size_bytes);

    //Fill the vectors with data
    for(int i = 0; i < size; i++) {
        h_a[i] = i;
        h_b[i] = i*2;
        hw_results[i] = 0;
        sw_results[i] = h_a[i] + h_b[i];
    }

//OPENCL HOST CODE AREA START
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    //Creating Context and Command Queue for selected device
    cl::Context context(device);
    cl::CommandQueue q(context, device);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>();
    std::cout << "Found Device=" << device_name.c_str() << std::endl;

    std::string vaddBinaryFile = xcl::find_binary_file(device_name,"krnl_vadd");
    cl::Program::Binaries vadd_bins = xcl::import_binary_file(vaddBinaryFile);
    devices.resize(1);
    cl::Program program(context, devices, vadd_bins);
    cl::Kernel krnl_vadd(program, "krnl_vadd");

    //Num sub_devices
    int num_sub_devices= N;
    std::vector<cl::Kernel> sub_kernel(num_sub_devices);

    //Specify device partition properties
    const cl_device_partition_property device_part_properties[3] = {
            CL_DEVICE_PARTITION_EQUALLY  ,
            1, // Use only one compute unit
            0 //CL_DEVICE_PARTITION_BY_COUNTS_LIST_END
    };

    //Create sub_devices, sub_context and corresponding queues
    std::vector<cl::Device> sub_devices(num_sub_devices);
    device.createSubDevices(device_part_properties, &sub_devices);

    cl::Context sub_context(sub_devices);
    std::vector<cl::CommandQueue> sub_q(num_sub_devices);
    cl::Buffer *d_a[num_sub_devices];
    cl::Buffer *d_b[num_sub_devices];
    cl::Buffer *d_output[num_sub_devices];

    for (int i = 0; i < num_sub_devices; i++ ) {
        //Determines each sub_devices data offset for a, b and result
        size_t offset = i * elements_per_subdevice;
        sub_q[i] = cl::CommandQueue(sub_context, device);
        sub_kernel[i] = cl::Kernel(program, "krnl_vadd");

        //Allocates memory on the FPGA DDR memory
        d_a[i] = new cl::Buffer(sub_context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                        size_per_subdevice, &h_a[offset], NULL);
        d_b[i] = new cl::Buffer(sub_context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                        size_per_subdevice, &h_b[offset], NULL);
        d_output[i] = new cl::Buffer(sub_context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                                        size_per_subdevice, &hw_results[offset], NULL);

        cl::Event write_event;
        cl::Event task_event;

        std::vector<cl::Event> wait_write_event;
        std::vector<cl::Event> wait_task_event;

        //Copy the data to the device memory
        sub_q[i].enqueueMigrateMemObjects({*(d_a[i]), *(d_b[i])}, 0, NULL, &write_event);

        //Set the kernel arguments
        krnl_vadd.setArg(0, *(d_a[i]));
        krnl_vadd.setArg(1, *(d_b[i]));
        krnl_vadd.setArg(2, *(d_output[i]));
        krnl_vadd.setArg(3, elements_per_subdevice);

        wait_write_event.push_back(write_event);
        //Launch the kernel
        sub_q[i].enqueueTask(krnl_vadd, &wait_write_event, &task_event);

        wait_task_event.push_back(task_event);
        //Copy back the result and this call will write the data from the d_output
        //buffer object to the source hw_result vector
        sub_q[i].enqueueMigrateMemObjects({*(d_output[i])}, CL_MIGRATE_MEM_OBJECT_HOST, &wait_task_event, NULL);
    }

    for(int i = 0; i < num_sub_devices; i++){
        //Wait for all the commands to finish in the queue
        sub_q[i].finish();
    }

    //Compare the device results with software results
	bool match = true;
	for (int i = 0; i < size; i++) {
	    if (sw_results[i] != hw_results[i]){
	        printf("%d a = %d b = %d \t sw = %d \t hw = %d\n", i, h_a[i], h_b[i], sw_results[i], hw_results[i]);
	        match = false;
            break;
	    }
	}

    std::cout << "TEST" << (match ? "PASSED" : "FAILED") << std::endl;
    return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}
