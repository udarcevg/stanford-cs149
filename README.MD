

## Notes

### Program 1.1
Multithreading parallelizes work across multiple CPU cores or hardware threads, while SIMS
parallelizes operations on multiple data elements using vector register in a single instruction.

* Multithreading = many workers
* SIMD = one worker with many hands


### Program 2.1
I ran ./myexpr -s  10000 with vectors widths 2, 4, 8 and 16. As the vector width increased, vector
utilization decreased. This happens because wider vectors have more lanes, but some lines  become inactive due
to masking, especially in the final partial vector iteration or in conditional code. With a large vector
width, the same amount of leftover work can waste more SIMD lanes, reducing overall utilization.

VECTOR_WIDTH | Vector utilization
2            | 89.1%
4            | 84.2%
8            | 85.8%
16           | 80.3%

### Program 3 Part 1

The ideal maximum speedup is about 8x because ISPC is configured to generate
8-wide AVX2 SIMD instructions. In this run, the serial version took 194.482ms and
the ISPC version took 53.869 ms, giving a speedup of 3.61x.

The observed speedup is lower than the ideal because the Mandelbrot 
computation has divergent control flow. Different pixels requre different
numrs of iterations  before they escape. Pixel far outside the side escape
quickly, while pixel near the boundary or inside the set may require 
many more iterations. Since SIMD lanes execute together, lanes that finish 
early become inactive while waiting for slower lanes in the same vector group.

The most challenging of the image are near the boundary of the Mandelbrot set,
where neighboring pixels can have different iterations count. Views with
more boundary detail usually show lower SIMD efficiency and lower speedup. 

Wrote image file mandelbrot-serial.ppm
[mandelbrot ispc]:              [53.869] ms
Wrote image file mandelbrot-ispc.ppm
(3.61x speedup from ISPC)

### Program 3 Part 2

1.
```
[mandelbrot serial]:            [192.674] ms
Wrote image file mandelbrot-serial.ppm
[mandelbrot ispc]:              [53.632] ms
Wrote image file mandelbrot-ispc.ppm
[mandelbrot multicore ispc]:    [28.262] ms
Wrote image file mandelbrot-task-ispc.ppm
(3.59x speedup from ISPC)
(6.82x speedup from task ISPC)
```

2. ...

3. ISPC tasks are lightweight units of work executed by a fixed pool if worker threads, 
while C++ threads are operating system threads. Launch 10,000 OS thread create significant
scheduling and memory overhead. In contrast, launching 10,000 ISPC tasks only add work to
a task queue, and the existing worker threads execute the task efficiently. Therefore,ISPC task 
scale much better for a large number of small task. 

### Program 4

1.
```
[sqrt serial]:          [82.150] ms
[sqrt ispc]:            [41.383] ms
[sqrt task ispc]:       [8.955] ms
[sqrt neon intrinsics]: [8.386] ms
(1.99x speedup from ISPC)
(9.17x speedup from task ISPC)
(9.80x speedup from Neon Intrinsics)
(1.99x speedup from ISPC)
(9.17x speedup from task ISPC)
```

2. To maximize ISPC speedup, I changed the input array so that every element has the same value:
```value[i]=1.0f```. This improves SIMD speedup every SIMD lane performs the same amount of work.
3.
```
for (unsigned int i = 0; i < N; i++) {
if (i % 8 == 0)
values[i] = 0.001f;   // slow
else
values[i] = 1.0f;     // fast
}
```

To minimize the speedup of the ISPC implementation without task, I constructed an input where each group 
of 8 values contains one expensive element and 7 cheaper.

SIMD lanes: [slow, fast, fast, fast, fast, fast, fast, fast]