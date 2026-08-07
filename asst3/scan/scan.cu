#include <stdio.h>

#include <cuda.h>
#include <cuda_runtime.h>

#include <driver_functions.h>

#include <thrust/scan.h>
#include <thrust/device_ptr.h>
#include <thrust/device_malloc.h>
#include <thrust/device_free.h>

#include "CycleTimer.h"

#define THREADS_PER_BLOCK 256


// helper function to round an integer up to the next power of 2
static inline int nextPow2(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}


// for (int stride = 1; stride <= N / 2; stride *= 2) {
//     int blockSize = 2 * stride;
//
//     parallel_for (int i = 0; i < N; i += blockSize) {
//         output[i + blockSize - 1] += output[i + stride - 1];
//     }
// }
// Source:               1   2   3   4   5   6   7   8
//                        \ /     \ /     \ /     \ /
// blockSize = 2:          3       7       11      15
//                         \     /          \     /
// blockSize = 4:             10               26
//                             \             /
// blockSize = 8:                     36

__global__ void upsweepKernel(
    int *data,
    int N,
    int stride,
    int numOps)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int step = blockDim.x * gridDim.x;

    for (int op = tid; op < numOps; op += step) {
        int right = (op + 1) * stride * 2 - 1;
        int left = right - stride;

        data[right] += data[left];
    }
}


__global__ void downsweepKernel(
    int *data,
    int N,
    int stride,
    int numOps)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int step = blockDim.x * gridDim.x;

    for (int op = tid; op < numOps; op += step) {
        int right = (op + 1) * stride * 2 - 1;
        int left = right - stride;

        int temp = data[left];
        data[left] = data[right];
        data[right] += temp;
    }
}


__global__ void clearLastKernel(
    int *data,
    int N
) {
    if (blockIdx.x == 0 && threadIdx.x == 0) {
        data[N - 1] = 0;
    }
}

// exclusive_scan --
//
// Implementation of an exclusive scan on global memory array `input`,
// with results placed in global memory `result`.
//
// N is the logical size of the input and output arrays, however
// students can assume that both the start and result arrays we
// allocated with next power-of-two sizes as described by the comments
// in cudaScan().  This is helpful, since your parallel scan
// will likely write to memory locations beyond N, but of course not
// greater than N rounded up to the next power of 2.
//
// Also, as per the comments in cudaScan(), you can implement an
// "in-place" scan, since the timing harness makes a copy of input and
// places it in result
void exclusive_scan(int *input, int N, int *result) {
    if (N <= 0)
        return;

    const int threadsPerBlock = THREADS_PER_BLOCK;
    const int scanSize = nextPow2(N);

    if (input != result) {
        cudaMemcpy(
            result,
            input,
            N * sizeof(int),
            cudaMemcpyDeviceToDevice);
    }

    if (scanSize > N) {
        cudaMemset(
            result + N,
            0,
            (scanSize - N) * sizeof(int));
    }

    if (scanSize == 1) {
        cudaMemset(result, 0, sizeof(int));
        return;
    }

    // Upsweep
    for (int stride = 1; stride < scanSize; stride *= 2) {
        int numOps = scanSize / (2 * stride);

        int numBlocks = (numOps + threadsPerBlock - 1) / threadsPerBlock;

        upsweepKernel<<<numBlocks, threadsPerBlock>>>(
            result, scanSize, stride, numOps);
    }

    cudaMemset(result + scanSize - 1,0,sizeof(int));

    // Downsweep
    for (int stride = scanSize / 2; stride >= 1; stride /= 2) {
        int numOps = scanSize / (2 * stride);

        int numBlocks = (numOps + threadsPerBlock - 1) / threadsPerBlock;
        downsweepKernel<<<numBlocks, threadsPerBlock>>>(
            result, scanSize, stride, numOps);
    }
}


//
// cudaScan --
//
// This function is a timing wrapper around the student's
// implementation of scan - it copies the input to the GPU
// and times the invocation of the exclusive_scan() function
// above. Students should not modify it.
double cudaScan(int *inarray, int *end, int *resultarray) {
    int *device_result;
    int *device_input;
    int N = end - inarray;

    // This code rounds the arrays provided to exclusive_scan up
    // to a power of 2, but elements after the end of the original
    // input are left uninitialized and not checked for correctness.
    //
    // Student implementations of exclusive_scan may assume an array's
    // allocated length is a power of 2 for simplicity. This will
    // result in extra work on non-power-of-2 inputs, but it's worth
    // the simplicity of a power of two only solution.

    int rounded_length = nextPow2(end - inarray);

    cudaMalloc((void **) &device_result, sizeof(int) * rounded_length);
    cudaMalloc((void **) &device_input, sizeof(int) * rounded_length);

    // For convenience, both the input and output vectors on the
    // device are initialized to the input values. This means that
    // students are free to implement an in-place scan on the result
    // vector if desired.  If you do this, you will need to keep this
    // in mind when calling exclusive_scan from find_repeats.
    cudaMemcpy(device_input, inarray, (end - inarray) * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(device_result, inarray, (end - inarray) * sizeof(int), cudaMemcpyHostToDevice);

    double startTime = CycleTimer::currentSeconds();

    exclusive_scan(device_input, N, device_result);

    // Wait for completion
    cudaDeviceSynchronize();
    double endTime = CycleTimer::currentSeconds();

    cudaMemcpy(resultarray, device_result, (end - inarray) * sizeof(int), cudaMemcpyDeviceToHost);

    double overallDuration = endTime - startTime;
    return overallDuration;
}


// cudaScanThrust --
//
// Wrapper around the Thrust library's exclusive scan function
// As above in cudaScan(), this function copies the input to the GPU
// and times only the execution of the scan itself.
//
// Students are not expected to produce implementations that achieve
// performance that is competition to the Thrust version, but it is fun to try.
double cudaScanThrust(int *inarray, int *end, int *resultarray) {
    int length = end - inarray;
    thrust::device_ptr<int> d_input = thrust::device_malloc<int>(length);
    thrust::device_ptr<int> d_output = thrust::device_malloc<int>(length);

    cudaMemcpy(d_input.get(), inarray, length * sizeof(int), cudaMemcpyHostToDevice);

    double startTime = CycleTimer::currentSeconds();

    thrust::exclusive_scan(d_input, d_input + length, d_output);

    cudaDeviceSynchronize();
    double endTime = CycleTimer::currentSeconds();

    cudaMemcpy(resultarray, d_output.get(), length * sizeof(int), cudaMemcpyDeviceToHost);

    thrust::device_free(d_input);
    thrust::device_free(d_output);

    double overallDuration = endTime - startTime;
    return overallDuration;
}


__global__ void markRepeatsKernel(
    const int *input,
    int length,
    int roundedLength,
    int *repeatFlags
) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid >= roundedLength) {
        return;
    }

    if (tid < length - 1 && input[tid] == input[tid + 1]) {
        repeatFlags[tid] = 1;
    } else {
        // This also initializes the padding and the final logical element.
        repeatFlags[tid] = 0;
    }
}

__global__ void scatterRepeatsKernel(
    const int *repeatFlags,
    const int *repeatPositions,
    int length,
    int *output
) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid < length - 1 && repeatFlags[tid] == 1) {
        int outputIndex = repeatPositions[tid];
        output[outputIndex] = tid;
    }
}

// find_repeats --
//
// Given an array of integers `device_input`, returns an array of all
// indices `i` for which `device_input[i] == device_input[i+1]`.
//
// Returns the total number of pairs found
int find_repeats(int *device_input, int length, int *device_output) {
    if (length <= 1) {
        return 0;
    }

    const int roundedLength = nextPow2(length);
    const int blocks = (roundedLength + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;

    int *device_repeat_flags = nullptr;
    int *device_repeat_position = nullptr;

    cudaMalloc(reinterpret_cast<void **>(&device_repeat_flags), roundedLength * sizeof(int));
    cudaMalloc(reinterpret_cast<void **>(&device_repeat_position), roundedLength * sizeof(int));

    // Step 1
    // flags[i] = 1 if input[i] == input[i + 1]
    // flags = 0 // otherwise
    markRepeatsKernel<<<blocks, THREADS_PER_BLOCK>>>(
        device_input,
        length,
        roundedLength,
        device_repeat_flags
    );

    // Step 2: Compute where each repeated index should be written.
    // flags:      0, 1, 0, 1, 1, 0
    // position    0, 0, 1, 1, 2, 3
    exclusive_scan(
        device_repeat_flags,
        roundedLength,
        device_repeat_position
    );

    // Since flags[length - 1] is always zero, the exclusive-scan
    // value at length - 1 equals the total numbers of repeated pairs.
    int repeatCount = 0;
    int lastFlag = 0;

    cudaMemcpy(&repeatCount, device_repeat_position + length - 1, sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(&lastFlag, device_repeat_flags + length - 1, sizeof(int), cudaMemcpyDeviceToHost);

    repeatCount += lastFlag;
    // Step 3: Compact the repeated indices into device_output.
    scatterRepeatsKernel<<<blocks, THREADS_PER_BLOCK>>>(
        device_repeat_flags,
        device_repeat_position,
        length,
        device_output
    );

    cudaFree(device_repeat_flags);
    cudaFree(device_repeat_position);
    return repeatCount;
}


//
// cudaFindRepeats --
//
// Timing wrapper around find_repeats. You should not modify this function.
double cudaFindRepeats(int *input, int length, int *output, int *output_length) {
    int *device_input;
    int *device_output;
    int rounded_length = nextPow2(length);

    cudaMalloc((void **) &device_input, rounded_length * sizeof(int));
    cudaMalloc((void **) &device_output, rounded_length * sizeof(int));
    cudaMemcpy(device_input, input, length * sizeof(int), cudaMemcpyHostToDevice);

    cudaDeviceSynchronize();
    double startTime = CycleTimer::currentSeconds();

    int result = find_repeats(device_input, length, device_output);

    cudaDeviceSynchronize();
    double endTime = CycleTimer::currentSeconds();

    // set output count and results array
    *output_length = result;
    cudaMemcpy(output, device_output, length * sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(device_input);
    cudaFree(device_output);

    float duration = endTime - startTime;
    return duration;
}


void printCudaInfo() {
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);

    printf("---------------------------------------------------------\n");
    printf("Found %d CUDA devices\n", deviceCount);

    for (int i = 0; i < deviceCount; i++) {
        cudaDeviceProp deviceProps;
        cudaGetDeviceProperties(&deviceProps, i);
        printf("Device %d: %s\n", i, deviceProps.name);
        printf("   SMs:        %d\n", deviceProps.multiProcessorCount);
        printf("   Global mem: %.0f MB\n",
               static_cast<float>(deviceProps.totalGlobalMem) / (1024 * 1024));
        printf("   CUDA Cap:   %d.%d\n", deviceProps.major, deviceProps.minor);
    }
    printf("---------------------------------------------------------\n");
}
