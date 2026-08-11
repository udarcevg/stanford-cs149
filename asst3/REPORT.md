### Saxpy

```
---------------------------------------------------------
Found 1 CUDA devices
Device 0: Tesla T4
   SMs:        40
   Global mem: 14913 MB
   CUDA Cap:   7.5
---------------------------------------------------------
Running 3 timing tests:
Kernel execution time: 104.022 ms		[10.744 GB/s]
Effective BW by CUDA saxpy: 361.431 ms		[3.092 GB/s]
Kernel execution time: 4.590 ms		[243.487 GB/s]
Effective BW by CUDA saxpy: 293.785 ms		[3.804 GB/s]
Kernel execution time: 4.692 ms		[238.190 GB/s]
Effective BW by CUDA saxpy: 287.621 ms		[3.886 GB/s]
```

SAXPY:
`totalBytes = reads x[i] (4 bytes) + reads y[i] (4 bytes) + writes result[i] (4 bytes) = 12 bytes/ops`

```
N = 100_000_000
time = 4.69 ms = 0.00469 s

bandwidth = N * 1.2  = 100_000_000 * 1.2 = 1.2 * 10^9 / 0.00469 s = 2.56 * 10^11 = 256 GB/s
```


> **Note the Throughput Units:**
> 
> The bandwidth is 256 GB/s using the standard decimal notation. Kb = 1000 bytes
>
> If measured in binary units 238.42 GiB/s. Kb = 1024 bytes


The first kernel execution was significantly slower than the following executions: 104 ms
compared with approximately 4.6 ms. This is caused by one-time CUDA startup costs, such as
loading the kernel and initializing runtime state. Therefore, the later runs better 
represent steady-state performance.

The sable kernel runtime was approximately 4.64 ms, corresponding to about 241 GB/s of device 
memory bandwidth. SAXPY in memory-bandwidth bound because each element requires two 
floating-point operations but transfer 12 bytes: 2 * 4 bytes read and 1 * 4 bytes write.

The complete CUDA operation took approximately 290 ms, which is much longer the kernel itself.
Most of this time is caused by copying the input arrays from the CPU to the GPU and copying the
result back to the CPU. Thus, although the CUDA kernel is fast, host-device communication 
dominates the total runtime.


### Render 

A CUDA block owns a tile. Before shading pixels, the block cooperatively determines which cirecles
can intersect that tile.


```
Rendering to 1024x1024 image 
Loaded scene with 10000 circles
Loaded scene with 10000 circles
---------------------------------------------------------
Initializing CUDA for CudaRenderer
Found 1 CUDA devices
Device 0: NVIDIA GeForce RTX 3090
SMs:        82
Global mem: 24575 MB
CUDA Cap:   8.6
---------------------------------------------------------

Running benchmark, 1 frames, beginning at frame 0 ...
Dumping frames to output_xxx.ppm
Copying image data from device
Wrote image file output_0000.ppm
Copying image data from device
***************** Correctness check passed **************************
Clear:    1.0341 ms
Advance:  0.0006 ms
Render:   14.0180 ms
Total:    15.0526 ms
File IO:  28.6175 ms
```

```
Rendering to 1024x1024 image
Loaded scene with 10000 circles
Loaded scene with 10000 circles
---------------------------------------------------------
Initializing CUDA for CudaRenderer
Found 1 CUDA devices
Device 0: NVIDIA GeForce RTX 3090
   SMs:        82
   Global mem: 24575 MB
   CUDA Cap:   8.6
---------------------------------------------------------

Running benchmark, 1 frames, beginning at frame 0 ...
Dumping frames to output_xxx.ppm
Copying image data from device
Wrote image file output_0000.ppm
Copying image data from device
***************** Correctness check passed **************************
Clear:    1.0328 ms
Advance:  0.0006 ms
Render:   11.2220 ms
Total:    12.2555 ms
File IO:  33.5196 ms

Overall:  0.3057 sec (note units are seconds)
```

```
Rendering to 1024x1024 image
Loaded scene with 10000 circles
Loaded scene with 10000 circles
---------------------------------------------------------
Initializing CUDA for CudaRenderer
Found 1 CUDA devices
Device 0: NVIDIA GeForce RTX 3090
   SMs:        82
   Global mem: 24575 MB
   CUDA Cap:   8.6
---------------------------------------------------------

Running benchmark, 1 frames, beginning at frame 0 ...
Dumping frames to output_xxx.ppm
Copying image data from device
Wrote image file output_0000.ppm
Copying image data from device
***************** Correctness check passed **************************
Clear:    1.0979 ms
Advance:  0.0006 ms
Render:   1.2930 ms
Total:    2.3915 ms
File IO:  33.0492 ms

Overall:  0.2911 sec (note units are seconds)
```

