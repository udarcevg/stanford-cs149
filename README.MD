

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


### Program 5

1.
```
[saxpy ispc]:           [3.143] ms      [94.831] GB/s   [12.728] GFLOPS
[saxpy task ispc]:      [2.802] ms      [106.345] GB/s  [14.273] GFLOPS
```
The SAXPY computations is trivial parallel and has regular memory access, but its performance
is mainly limited by memory bandwidth rather than arithmetic throughput. Using ISPC with tasks 
improves performance because multiple cores can process different parts of the array in parallel.

2.
The multiplier of 4 is correct because writing to the result array generates both a read and
a write to memory. When a cache line of result is not already in the cache, the CPU first loads it into
the cache (write-allocation). After the values are updated, the  modified cache line is eventually
written back to memory. The total memory traffic is approximately 
```
    Load X + 
    Load Y + 
    Read result cache line (write-allocate) + 
    Write result cache line  = 
    4 memory transfers = 
    4 * N * sizeof(float) bytes of memory traffic.
```

3. 
```
[saxpy serial]:         [3.213] ms      [92.758] GB/s   [12.450] GFLOPS
[saxpy ispc]:           [2.780] ms      [107.193] GB/s  [14.387] GFLOPS
[saxpy task ispc]:      [3.473] ms      [85.811] GB/s   [11.517] GFLOPS
[saxpy neon]:           [2.796] ms      [106.594] GB/s  [14.307] GFLOPS
                                (0.80x speedup from use of tasks)
                                (1.15x speedup from NEON)
```


### Program 6

1. 
```
./kmeans
Reading data.dat...
Running K-means with: M=1000000, N=100, K=3, epsilon=0.100000
[Total Time]: 7366.597 ms
```
2. I added timing measurements around computeAssignments, computeCentroids, and computeCost.
The result showed that most of the runtime was spent in computeAssignments. This is makes sense
because computeAssignments computes the distance from every data point to every centroid, so its
work proportional M * K * N.
```
computeAssignments: ~111 ms
computeCentroids:   ~14 ms
computeCost:        ~26 ms
```
3. Based on this, I parallelized computeAssignments. This was a good target because each data point
can be assigned independently. I divided the data points among multiple threads, and each thread 
computed the closest centroid for its own range of points. No synchronization was needed because
each thread wrote to a different part of the cluster Assignments array.
```angular2html
[Total Time]: 7424.952 ms - OLD
[Total Time]: 67.733 ms - NEW
```