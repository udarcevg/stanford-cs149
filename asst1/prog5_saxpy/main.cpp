#include <stdio.h>
#include <algorithm>
#include <arm_neon.h>
#include "CycleTimer.h"
#include "saxpy_ispc.h"

extern void saxpySerial(int N, float a, float* X, float* Y, float* result);


// return GB/s
static float
toBW(int bytes, float sec) {
    return static_cast<float>(bytes) / (1024. * 1024. * 1024.) / sec;
}

static float
toGFLOPS(int ops, float sec) {
    return static_cast<float>(ops) / 1e9 / sec;
}

static void verifyResult(int N, float* result, float* gold) {
    for (int i=0; i<N; i++) {
        if (result[i] != gold[i]) {
            printf("Error: [%d] Got %f expected %f\n", i, result[i], gold[i]);
        }
    }
}

using namespace ispc;



void saxpyNeon(int N, float scale, float* X, float* Y, float* result) {
    float32x4_t vscale = vdupq_n_f32(scale);

    int i = 0;

    for (; i <= N - 4; i += 4) {
        float32x4_t vx = vld1q_f32(&X[i]);
        float32x4_t vy = vld1q_f32(&Y[i]);

        // result = scale * X + Y
        float32x4_t vres = vmlaq_f32(vy, vx, vscale);

        vst1q_f32(&result[i], vres);
    }

    // cleanup for remaining elements
    for (; i < N; i++) {
        result[i] = scale * X[i] + Y[i];
    }
}


int main() {

    const unsigned int N = 20 * 1000 * 1000; // 20 M element vectors (~80 MB)
    const unsigned int TOTAL_BYTES = 4 * N * sizeof(float);
    const unsigned int TOTAL_FLOPS = 2 * N;

    float scale = 2.f;

    float* arrayX = new float[N];
    float* arrayY = new float[N];
    float* resultSerial = new float[N];
    float* resultISPC = new float[N];
    float* resultTasks = new float[N];
    float* resultNeon = new float[N];

    // initialize array values
    for (unsigned int i=0; i<N; i++)
    {
        arrayX[i] = i;
        arrayY[i] = i;
        resultSerial[i] = 0.f;
        resultISPC[i] = 0.f;
        resultTasks[i] = 0.f;
        resultNeon[i] = 0.f;
    }

    //
    // Run the serial implementation. Repeat three times for robust
    // timing.
    //
    double minSerial = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime =CycleTimer::currentSeconds();
        saxpySerial(N, scale, arrayX, arrayY, resultSerial);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
    }

    printf("[saxpy serial]:\t\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
              minSerial * 1000,
              toBW(TOTAL_BYTES, minSerial),
              toGFLOPS(TOTAL_FLOPS, minSerial));


    // Run the ISPC (single core) implementation
    //
    double minISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpy_ispc(N, scale, arrayX, arrayY, resultISPC);
        double endTime = CycleTimer::currentSeconds();
        minISPC = std::min(minISPC, endTime - startTime);
    }

    verifyResult(N, resultISPC, resultSerial);

    printf("[saxpy ispc]:\t\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minISPC * 1000,
           toBW(TOTAL_BYTES, minISPC),
           toGFLOPS(TOTAL_FLOPS, minISPC));

    //
    // Run the ISPC (multi-core) implementation
    //
    double minTaskISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpy_ispc_withtasks(N, scale, arrayX, arrayY, resultTasks);
        double endTime = CycleTimer::currentSeconds();
        minTaskISPC = std::min(minTaskISPC, endTime - startTime);
    }

    printf("[saxpy task ispc]:\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minTaskISPC * 1000,
           toBW(TOTAL_BYTES, minTaskISPC),
           toGFLOPS(TOTAL_FLOPS, minTaskISPC));


    double minNeon = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpyNeon(N, scale, arrayX, arrayY, resultNeon);
        double endTime = CycleTimer::currentSeconds();
        minNeon = std::min(minNeon, endTime - startTime);
    }
    verifyResult(N, resultNeon, resultSerial);
    printf("[saxpy neon]:\t\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
       minNeon * 1000,
       toBW(TOTAL_BYTES, minNeon),
       toGFLOPS(TOTAL_FLOPS, minNeon));

    printf("\t\t\t\t(%.2fx speedup from use of tasks)\n", minISPC/minTaskISPC);
    printf("\t\t\t\t(%.2fx speedup from NEON)\n", minSerial / minNeon);
    //printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    //printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);

    delete[] arrayX;
    delete[] arrayY;
    delete[] resultSerial;
    delete[] resultISPC;
    delete[] resultTasks;
    delete[] resultNeon;

    return 0;
}
