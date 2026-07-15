#include <stdio.h>
#include <algorithm>
#include <pthread.h>
#include <math.h>

#include "CycleTimer.h"
#include "sqrt_ispc.h"
#include <arm_neon.h>

using namespace ispc;

extern void sqrtSerial(int N, float startGuess, float* values, float* output);

static void verifyResult(int N, float* result, float* gold) {
    for (int i=0; i<N; i++) {
        if (fabs(result[i] - gold[i]) > 1e-4) {
            printf("Error: [%d] Got %f expected %f\n", i, result[i], gold[i]);
        }
    }
}

void sqrtNeon(int N, float initialGuess, float values[], float output[]) {
    for (int i = 0; i < N; i += 8) {
        float32x4_t x_0 = vld1q_f32(&values[i]);
        float32x4_t x_1 = vld1q_f32(&values[i + 4]);

        // float32x4_t guess_0 = vdupq_n_f32(initialGuess);
        // float32x4_t guess_1 = vdupq_n_f32(initialGuess);
        float32x4_t guess_0 = vrsqrteq_f32(x_0);
        float32x4_t guess_1 = vrsqrteq_f32(x_1);

        float32x4_t v_three = vdupq_n_f32(3.f);
        float32x4_t v_half = vdupq_n_f32(0.5f);

        // 6 итераций за глаза хватает для точности float
        for (int iter = 0; iter < 4; iter++) {
            float32x4_t g2_0 = vmulq_f32(guess_0, guess_0);
            float32x4_t g3_0 = vmulq_f32(g2_0, guess_0);
            float32x4_t term_0 = vmulq_f32(x_0, g3_0);
            float32x4_t sub0 = vsubq_f32(vmulq_f32(v_three, guess_0), term_0);
            guess_0 = vmulq_f32(v_half, sub0);

            float32x4_t g2_1 = vmulq_f32(guess_1, guess_1);
            float32x4_t g3_1 = vmulq_f32(g2_1, guess_1);
            float32x4_t term_1 = vmulq_f32(x_1, g3_1);
            float32x4_t sub1 = vsubq_f32(vmulq_f32(v_three, guess_1), term_1);
            guess_1 = vmulq_f32(v_half, sub1);
        }

        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ:
        // Превращаем обратный корень (1/sqrt(x)) в обычный корень (sqrt(x))
        // Умножаем финальный guess на исходный x
        float32x4_t final_res_0 = vmulq_f32(guess_0, x_0);
        float32x4_t final_res_1 = vmulq_f32(guess_1, x_1);

        // Сохраняем правильный результат
        vst1q_f32(&output[i], final_res_0);
        vst1q_f32(&output[i+4], final_res_1);
    }
}

int main() {

    const unsigned int N = 20 * 1000 * 1000;
    const float initialGuess = 1.0f;

    float* values = new float[N];
    float* output = new float[N];
    float* gold = new float[N];

    for (unsigned int i=0; i<N; i++)
    {
        // TODO: CS149 students.  Attempt to change the values in the
        // array here to meet the instructions in the handout: we want
        // to you generate best and worse-case speedups
        
        // starter code populates array with random input values
        values[i] = .001f + 2.998f * static_cast<float>(rand()) / RAND_MAX;
        // Тяжелый элемент встречается редко (всего ~3% или ~1.5% элементов)
        // if (i % 32 == 0) {
        //     values[i] = 2.998f;
        // } else {
        //     values[i] = 1.0f;
        // }
    }

    std::sort(values, values + N);

    // generate a gold version to check results
    for (unsigned int i=0; i<N; i++)
        gold[i] = sqrt(values[i]);

    //
    // And run the serial implementation 3 times, again reporting the
    // minimum time.
    //
    double minSerial = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrtSerial(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
    }

    printf("[sqrt serial]:\t\t[%.3f] ms\n", minSerial * 1000);

    verifyResult(N, output, gold);

    //
    // Compute the image using the ispc implementation; report the minimum
    // time of three runs.
    //
    double minISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrt_ispc(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minISPC = std::min(minISPC, endTime - startTime);
    }

    printf("[sqrt ispc]:\t\t[%.3f] ms\n", minISPC * 1000);

    verifyResult(N, output, gold);

    // Clear out the buffer
    for (unsigned int i = 0; i < N; ++i)
        output[i] = 0;

    //
    // Tasking version of the ISPC code
    //
    double minTaskISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrt_ispc_withtasks(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minTaskISPC = std::min(minTaskISPC, endTime - startTime);
    }

    printf("[sqrt task ispc]:\t[%.3f] ms\n", minTaskISPC * 1000);


    float* output_neon = new float[N];
    for (int i = 0; i < N; i++) output_neon[i] = 0; // Исправлено: зануляем output_neon

    double minNeon = 1e30;
    for (int i = 0; i < 3; i++) {
        double startTime = CycleTimer::currentSeconds();
        sqrtNeon(N, initialGuess, values, output_neon);
        double endTime = CycleTimer::currentSeconds();
        minNeon = std::min(minNeon, endTime - startTime);
    }

    printf("[sqrt neon intrinsics]: [%.3f] ms\n", minNeon * 1000);

    // Исправлено: проверяем ИМЕННО output_neon, а не пустой output
    verifyResult(N, output_neon, gold);

    printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);
    // Можно также добавить вывод ускорения для Neon:
    printf("\t\t\t\t(%.2fx speedup from Neon Intrinsics)\n", minSerial/minNeon);

    printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);

    delete [] values;
    delete [] output;
    delete [] gold;
    delete[] output_neon;

    return 0;
}

