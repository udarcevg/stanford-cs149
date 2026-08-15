## PROBLEM 1

### 1.A

The blur os performed in two passes.

```
for (int ii=0; ii<3; ii++)
    tmp += input[..] * weights[ii] // 3 multiplies  + 3 adds = 6 FLOPS
```

In the horizontal pass, each pixel requires 6 FLOPs (3 multiplications and 3 additions). The input pixel is read from
memory (4 bytes), and thr result is written to tmo_buf (4 bytes). Therefore, the horizontal pass transfer 8 bytes per 
pixel.

In the vertical pass, each pixel again requires 6 FLOPs. A value is read from tmp_buf (4 bytes), and the final result 
written to output (4 bytes). Therefore, the vertical pass transfers another 8 bytes per pixel.

```
Total FLOPs = 6 + 6 = 12

Total memory traffic = input read 4 + tmp_buf write 4 + tmp_buf_read 4 + output write 4 = 16 bytes

Arithmetic intensity =  12 FLOPs / 16 bytes = 0.75 FLOPs/byte.
```

The main source of extra memory traffic os tmp_buf: the horizontal pass writes it to memory, and the vertical pass later
reads it back.

### 1.B

From Part A, the arithmetic intensity is 0.75 FLOPs/byte.
The memory-bandwidth-limited performance is: `0.75 FLOPs/byte * 1 TB/s = 0.75 TFLOPS`
The processor's compute limit is 1 TFLOPS, so the achievable performance is: `min(0.75 TFLOPS, 1 TFLOPS) = 0.75 FLOPS`
Therefore, the blur is memory-badwidth bound, and its peak performance is 0.75 FLOPS.


### 1.C

The intermediate tmp_buf can be replaced by an on-chip line buffer shared between the horizontal and vertical blur 
threads.

The vertical blur requires three complete rows to compute one output row. However, 3-row line buffer would become full
while the vertical thread is processing those rows, forcing the horizontal thread to stall. Therefore, one additional 
row is required so that the horizontal thread can produce the next row while the vertical thread consumes the current 
three rows.

`The minimum line buffer size = 4`

Thread 1 computes the horizontal blur and writes each result directly to the line buffer using enq() instead of writing 
tmp_bug.

Thread 2 waits until three rows are available, computes one output row using line_buf.access(i, [1,2,3]), and then call 
deqline() to discard the oldest row.

With four rows, both threads can make forward progress simultaneously in steady state, while the intermediate results 
remain on-chip instead of being written to off-chip memory.

```c++

LINEBUFF<float> line_buf[4][WIDTH];
line_buf.reset();
// horizontal
for (int j = 0; j < HEIGHT + 2; j++) {
    for (int i = 0; i < WIDTH; i ++) {
        float tmp = 0.f
        for int (ii = 0; ii < 3; ii++) {
            tmp += input[j*(WIDTH+2) + i + ii] * weight[ii];
        }
        line_buf.enq(tmp);
    }
}
// vertical
for (int j = 0; j < HEIGHT; j++) {
    line_buf.waitforrows(3);
    for (int i = 0; i < WIDTH; i ++) {
        float tmp = 0.f
        for int (jj = 0; jj < 3; jj++) {
            tmp += line_buf.access(i, jj) * weight[jj];
        }
        output[j * WIDTH + i] = tmp;
    }
    
    line_buf.deqline();
}
```


### 1.D

The computations still performs 12 FLOPs per output pixel.

With the on-chip line buffer, the intermediate values no longer generate off-chip memory traffic. The horizontal pass 
reads input (4 bytes) and writes to the on-chip line buffer (0 off-chip bytes) and writes output (4 bytes).
```
input read      = 4
line_buf write  = 0   (on-chip)
line_buf read   = 0   (on-chip)
output write    = 4
Total           = 8 bytes
```

`Arithmetic intensity = 12 / 8 = 1.5 FLOPs/byte`

Therefore, using the line buffer doubles the arithmetic intensity from 0.75 to 1.5 FLOPs/byte.

### 1.E
The memory-bandwidth = `1.5 FLOPs/Byte * 1 TB/s = 1.5 TFLOPS`
The processor's peak compute throughput is 1 FLOPS, so the achievable performance is: `min(1.5 FLOPS, 1 FLOPS) = 1 FLOPS`
Therefore, the optimized blur is compute-bound and can reach the processo's full peak performance of 1 FLOPS.


## 2


### 2.A

Each convolution performs FILTER_SIZE^2 math operations per output pixel. Since there are two convolutions, the total 
computation is:


```
// F * F = F^2 one convolution
for (jj = 0; jj < F; jj++)
    for (ii = 0; ii < F; ii++)
        accum += weight * input[...]; 
```
first convolution + second convolution = (2 * FILTER_SIZE^2 * HEIGHT * WIDTH) math operations.

Memory traffic occurs in the following order:

input read = HEIGHT * WIDTH elements
tmp write = HEIGHT * WIDTH elements
tmp read = HEIGHT * WIDTH elements
output write = HEIGHT * WIDTH elements

Therefore

```
Total memory traffic = 4 * HEIGHT * WIDTH elements
AI = (2 * FILTER_SIZE^2 * HEIGHT * WIDTH) / ( 4 * HEGHT * WIDTH) =  FILTER_SIZE^2 / 2 math operations per element.
```


### 2.B

To compute a CHUNK_SIZE * CHUNK_SIZE output region, the second convolution needs FILTER_SIZE elements for the first 
output pixel  and one additional temp element for each subsequent output pixel.

Therefore, the required width and height of temp are: `CHUNK_SIZE + FILTER_SIZE - 1`
So the total number of element in the temporary allocation is: `(CHUNK_SIZE + FILTER_SIZE - 1)^2`


### 2.C

For CHUNK_SIZE = 8 and FILTER_SIZE = 5, each chunk produces 8 ^ 2 final outputs pixels.

The required temp region is: `(8 + 5 - 1)^2 = 12^2`

Each temp element requires 5^2 math operations in the first convolution, so the first 
performs: `12^2 * 5^2 math operations`

The second convolution computes 8^2 output pixels, each requiring 5 ^ 2 math operations: `8^2 * 5^2 math operations`

Therefore, the total arithmetic per output pixel is: 
`(12^2 * 5^2 + 8^2 * 5^2) / 8^2 = 5200 math operations per output pixel.`

### 2.D

The transformation will hurt performance if the original program is already compute-bound. For FILTER_SIZE = 5, the
original implementation performs: `AI =  5^2 * 2 = 50 opx/pixel`

With chunking, from part C: `AI NEW = 5200 / 64 = 81.25 ops/pixel`.

Chunking reduces memory traffic by exploiting producer-consumer locality, but it also recomputes overlapping halo regions
between chunks. Since computation is already the bottleneck, reducing memory traffic does not improve performance, 
while the additional arithmetic makes the program slower.

### 2.E

The chunking transformation can ve useful in an energy-constrained system because it reduces off-chip memory traffic.

Without chunking, temp is written to memory after the first convolution and read back for the second convolution. With 
chunking, intermediate temp values can remain in local on-chip storage and be consumed immediately.

Although chunking performs additional arithmetic due to recomputation of overlapping halo regions, arithmetic operations
are generally much less energy-expensive than off-chip memory accesses. Therefore, reducing memory traffic can lower 
total energy consuption even if performance does not imporve.


## 3

### 3.A

Each element processed by EXP requires 10 FLOPs.

For each element, the EXP loops one float form A (4 bytes) and write one float back to A (4 bytes).

Total memory traffic per element = `4 + 4 = 8 bytes`

Arithmetic intensity = 10 FLOPs / 8 bytes = 1.25 FLOPs/byte.


## 3.B

For N = 1000, the matrix contains 1,000,00 elements.
The EXP loop performs:
``
N * N = 1000 * 1000 = 1,000,000 elements
 ``
Step EXP loop
```
Compute time = 10 FLOPs * 1,000,000 elements / 10 MFLOPS peak = 1.0 s.
Memory Traffic = 1,000,000 * (READ A 4 byte + WRITE A 4 bytes) = 8,000,000 bytes = 8 MB
Memory time = 8 MB / 10 MB = 0.8 s
```
Therefore, the EXP loop is compute-bound and takes 1.0 s.

Step row_sum
```
// row_sum performs 1 FLOP per element
Compute time = 1 FLOPs * 1,000,000 elements / 10 MFLOPS peak = 0.1 s.
Memory Traffic = 1,000,000 * READ A 4 byte = 4,000,000 bytes = 4 MB
Memory time = 4 MB / 10 MB = 0.4 s
```
Therefore, row_sum is memory-bonund and takes 0.4s,

Total execution time: 1.0 + 0.4 = 1.4 seconds.


### 3.C

The two loop can be fused so that row_sum consumes the buffered output of EXP directly.

```c++
auto out_sum(N);
for (i = 0; i < N; i++) {
    auto a_tile = LOAD_TILE(A, i, a_tile_shape);
    auto out = EXP(a_tile);
    auto out_tile = BUFFER(out);
    
    STORE_TILE(A, i, out_tile);
    out_sum[i] = row_sum(out_tile);
}

auto out_tile = BUFFER(out_sum);
STORE_TILE(B, out_tile);
```

In the original code, each element of A is read once for EXP, written back, and then read again for row_sum

The fused version eliminates the second read  of A because row_sum uses the buffered EXP result directly.

Memory traffic per element decreases form 12 bytes to 8 bytes, while the arithmetic remain 11 FLOPS per element. This
improves arithmetic intensity by exploiting producer-consumer locality.


### 3.D

For N = 1000, the matrix contains 1,000,000 elements.

The fused loop performs 10 FLOPs for EXP and 1 FLOP for row_sum:

```
Total computation = 1,000,000 * 11 = 11 MFLOPs.
Compute time = 11 MFLOPs / 10 MFLOS peak = 1.1s
Memory Traffic = 1,000,000 * (4 + 4) = 8 MB
Memory Time = 8 MB / 10 MB bandwidth = 0.8s
Execution = max(1.1, 0.8) = 1.1
```


### 3.E

The fused code can be further optimized using metapipelining. Instead of waiting for the current tile to finish 
before loading the next tile, we overlap loading tile i+1 with processing tile i.

```
PROLOGUE
    load first item

STEADY STATE
    load next item
    process current item
    advance

EPILOGUE
    process last item
```

```c++
// PROLOGUE
auto current_tile = LOAD_TILE(A, 0, a_tile_shape);


// STEADY STATE
for (int = 0; i < N - 1; i++) {
    auto next_tile = LOAD_TILE(A, i + 1, a_tile_shape);
    
    auto out = EXP(current_tile);
    auto out_tile = BUFFER(out);
    
    STORE_TILE(A, i, out_tile);
    out_sum[i] = row_sum(out_tile);
    
    current_tile = next_tile
}
// EPILOGUE
auto out = EXP(current_tile);
auto out_tile = BUFFER(out);

STORE_TILE(A, N - 1, out_tile);
out_sum[N - 1] = row_sum(out_tile);
```

This allows memory transfers and computation from different iterations to overlap while preserving the producer-consumer
locality from thr fused implementation.



### 3.F

For N = 1000, the matrix contains 1,000,000 elements.

The fused loop performs 10 FLOPs for EXP and 1 FLOP for row_sum:

```
EXP computation = 1,000,000 * 10 FLOPS = 10 MFLOPs.
EXP time = 10 MFLOPs / 10 MFLOS peak = 1 s
row_sum computation = 1,000,000 * 1 FLOPS = 1 MFLOPs.
row_sum time = 1 MFLOPs / 10 MFLOS peak = 0.1 s
Memory Traffic = 1,000,000 * (4 + 4) = 8 MB
Memory Time = 8 MB / 10 MB bandwidth = 0.8s
```

With metapipelining, memory transfer, EXP and row_sum for different rows can overlap. Therefore, execution time is 
determinated by the slowest pipline stage: `max(1.0, 0.1, 0.8) = 1.0 s`

Therefore, the metapipelining implementation takes approximately 1.0 second.


## PRACTICE PROBLEM 1

### 1.A

```c++
void write_segment_sizes(
    int* flags,
    int* num_segs,
    int* starts
    ) {
    if (flags[id] == 1) {
        starts[scan[id]] == id;
    }
}

void compute_size(
    int N,
    int num_segs,
    int* starts,
    int* sizes
) {
    if (id == num_segs - 1)
        sizes[id] = N - starts[id];
    else
        sizes[id] = starts[id + 1] - start[id];
}

void segment_size(
    int N,
    int* flogs,
    int* num_segs,
    int* seg_size
    ) {
    int scan[N];
    int starts[N];
    
    exclusive_scan(N, flags, scan);
    *num_segs = scan[N - 1] + flags[N- 1];
    write_segment_starts<<<N>>>(flags, scan, start);
    compute_size<<<*num_segs>>>(N, *num_egs, starts, seg_sizes);
    }
```

### 1.B

$$\text{Work} = \frac{S}{2} + \frac{S}{4} + \frac{S}{8} + \dots + 1 = S \cdot \left(1 - \frac{1}{S}\right) = S - 1 = \mathbf{O(S)}$$

```c++
void reduce_max(int* values, int stride, int count) {
    int i = id;
    if (i < count) {
        if left = i * 2 * stride;
        int right = left + stride;
        
        values[left] = max(values[left], value[right]);
    }
}

int largest_segment_size(int N, int* flags) {
    int num_segs;
    int seq_sizes[N];
    
    segment_sizes(N, flags, &num_segs, seg_sizes);
    
    int stride = 1;
    int count = num_segs / 2;
    
    while (count > 0) {
        reduce_max<<<count>>>(seq_sizes, stride, count);
        stride *= 2;
        count /= 2;
    }
    
    return seq_sizes[0];
}
```


## PRACTICE PROBLEM 2

```c++
void threadA() {
    int outValue[COUNT];
    int inValues[COUNT];
    
    HANDLE sendHandles[COUNT];
    HANDLE recvHandles[COUNT];
    
    // Start all sends and receives
    for (int i = 0; i < COUNT; i++) {
        outValues[i] = randomInt();
        sendHandles[i] = asyncSend(&outValues[i]);
        recvHandles[i] = asyncRecv(&inValues[i])
    }
    
    // Wait until all reurned message arrive.
    for (int i = 0; i < COUNT; i++) {
        while (!testRecvDone(recvHandles[i]))
    }
    
    // Verify that every returned value was originally se
    std::unordered_set<int> sentSet;
    for (int i=0; i < COUNT; i++) {
        sentSet.insert(outValues[i]);
    }
    
    bool allMath = true;
    for (int i = 0; i < COUNT; i++) {
        if (sentSet.find(inValues[i]) == sentSet.end()) {
            allMatch = false;
            break;
        }
    }
    if (allMatch) {
        print("DONE\n")
    }
}

void threadB() {
    int values[COUNT];
    HANDLE recvHandles[COUNT];
    HANDLE sendHandles[COUNT];
    bool sentBack[COUNT] = {false};
    
    // Start all recieves.
    for (int i = 0; i < COUNT; i++) {
        recvHandles[i] = asyncRecv(&values[i]);
    }
    
    // As soon as a receive finishes, immerdiately sent that value back
    int numSentBack = 0;
    while (numSetnBack < COUNT) {
        for (int i = 0; i < COUNT; i++) {
            if (!sentBack[i] && testRecvDone(recvHanles[i])) {
                sendHandles[i] = asyncSend(&values[i]);
                sentBack[i] = true;
                numSentBack++;
            }
        }
    }
    
    // Ensure all return sends complete before threadB exists.
    for (int i = 0; i < COUNT; i++) {
        while (!testSendDone(sendHandles[i])) {
            
        }
    }   
}
```

All sense and receives are initiated asynchronously so many network operations can be outstanding at the same time. 
Thread B send each value back as soon as its receives completes, rather than waiting for all receives.

Because messages may arrive out of order, Thread A verifies the returned values without assuming that receives index 
i corresponds to send index i. The uniqueness of values returned by randomInt() makes this verification possible.


## PRACTICE PROBLEM 3


### PRACTICE PROBLEM 3.A
Each core executes one floating-point instruction per cycle across a 32-wide warp.

Per-core throughput: `1  inst/cycle * 32 wide * 1GHz = 32 GFLOPS`
With 16 core: `32 GFLOPS * 16 core = 512 GLOPS`

Therefore, the peak floating-point throughput of PKPU2.0 is 512 GFLOPS, or 0.512 TFLOPS

### PRACTICE PROBLEM 3.B

No, the workload does not suffer from instruction stream divergence.

Each warp contains 32 consecutive CUDE threads. The input is arranged in groups of 32 positive values followed by 32 
negative values. Therefore, all threads within a warp always take the same branch.

Warp containing positive values execute f1(), while warps containing negative values execute f2(). Different warps may
take different branches without causing divergence. Divergence only occurs when threads within the same warp take 
different branches.

### PRACTICE PROBLEM 3.C

For one warp, arithmetic work is:

```
1 cycle for val2
1 cycle for comparison
14 cycles for either f1 or f2
```
Since there is no divergence, only one branch path executed.

Arithmetic cycles per warp = `1 + 1 + 14 = 16 cycles`

Each of the 32 threads reads one float and write one float:

Memory traffic per warp = `32 * (4 + 4) = 256 bytes`

AI = `16/256 = 1/16 arithmetic cycles per byte`

### PRACTICE PROBLEM 3.D

Each core supports 256 CUDA thread contexts.

Since each warp contains 32 thread: `256 / 32 = 8 resident warps per core`

A load has a latency of 50 cycles. While one warp waits for its load, the processor can schedule the other 7 warps, 
but this only provide about 7 cycles of latency hiding before returning to the stalled warp.

Since 7 cycles is much less than the 50-cycle load latency, the processor cannot completely hide the memory latency.

Therefore, PKPU2.0 does not have enough residen warps to hide all load latency.

### PRACTICE PROBLEM 3.E

Each warp requires 16 arithmetic cycles and transfer 256 bytes.

The maximum compute throughput is: `16 core * 1 GHz / 16 cycles per warp = 1 billion warps/s`

The maximum throughput allowed by memory bandwidth is: `128 GBs / 256 bytes per warp = 0.5 billion warps/s`

Since the memory system supports fewer warps per second than the compute hardware, the program is bandwidth-bound.

Process utilization = 0.5 / 1.0 = 50%.

Therefore, the PKPU2.0 is bandwidth-bound with approximately 50% processor utilization.

### PRACTICE PROBLEM 3.F

I would choose option - b, triple the memory bandwidth.

From Part E, the original processor is bandwidth-bound and achieves only 50% compute utilization. Its throughput is 
limited to 0.5 billion warps/s by the 128 GB/s memory system.

Tripling bandwidth to 384 GB/s would support up to 1.5 billion warps/s, but the compute hardware can only 1.0 billion 
waprs/s.

Therefore, the new performance becomes compute-bound at 1.0 billion warps/s.

`Speedup = 1.0 / 0.5 = 2x`

The other option do not remove the bandwidth bottleneck: more execution context mainly help latency hiding, the cache 
provides little benefit because the data has almost no reuse, and wider WIMD increases compute throughput without 
increasing memory bandwidth.

## PROBLEM 4

### PROBLEM 4.A

Each warp executes two initial loads: `2 cycles`

Because every group of 8 input exponents contains at least one value 8, every 32-thread  warp contains at least one 
thread with expValue = 8. Therefore, the warp executes all 8 loop iterations.

Each iteration perform two arithmetic instructions:
`
result *=  baseValue;
result *= 2.0f or result *=4.0f
`

So the loop takes: `8 * 2 = 16 cycles`

Finally, storing the result requires 1 cycle.

Total cycles per warp = `2 + 16 + 1 = 19 cycles`

### PRACTICE PROBLEM 4.B

Each thread transfers 12 bytes:

`4 bytes for inputExps load + 4 bytes for inputValues load + 4 bytes for resultValues sotre`

Memory traffic per warp is: `32  * 12 = 384 bytes`

Compute-limit throughput:

`8 core * 1GHz / 30 cycles per warp = 266.67 million warps/s.`

Bandwidth-limited throughput:

`32 GB/s / 384 bytes per wart = 83.3 million warps/s.`

Since 83.3 < 266.67, the program is memory-bandwidth bound.

Compute utilization = 83.33 / 266.67 = 31.25%

Therefore, the GPU spend approximately: `100% - 31.25% = 68.75` of its time waiting on memory.

### PRACTICE PROBLEM 4.C

Each warp begin by issuing two loads instructions. The second load is issued one cycles after the first and becomes 
ready 75 cycles later, so the warp cannot continue until cycle 76.

While one wart waits, other warp can issue their two loads. Each warp therefore contributed 2 cycles of independent 
work before it also waits.

Number of warps required = `ceil(75/2) = 38 warps`

Since each warp contains 32 CUDA threads: `38 * 32 = 1216 CUDA threads execution context`

Therefore, the minimum required to completely hide the load latency is 38 warps, or 1216 CUDA thread execution context.


## PRACTICE PROBLEM 4

I would fuse operations 1-3 into one pass and fuse select branch into a second pass.

```c++
float sum = 0.0f;

#pragma omp parallel for reduction(+:sum)
for (int i = 0; i < N; i++) {
    float t1 = src1[i] + src2[i];
    float t2 = t1 * src3[i];
    float t3 = t2 * src1[i];
    
    tmp3[i] = t3;
    sum += t2;
}

float x = sum / N

// 3 read + 1 write 
// add + mul + mul + sum-add = 4 FLOPs/el.

if (x > 10.0f) {
    #pragma opm parallel for
    for (int i = 0; i < N; i++) {
        float t1 = tmp3[i] * src1[i];
        float t2 = src[1] + t1;
        dst[i] = src1[i] + t2;
    }
} else {
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        float t1 = tmp3[i] + src2[i];
        float t2 = src2[i] * t1;
        dst[i] = src2[i] * t2;
    }
}
```

The first pass perform 4 FLOPs and transfers 16 bytes per element. The second pass performs 3 FLOPs and transfer 12 
bytes per element.

`AI = 7 FLOPS / 12 byte = 0.25 FLOPs/byte.`

Computer A:
* Peak compute performance: `4 core * 1 GHz * 4 wide = 16 GFLOPS`
* Its bandwidth limit is: `192 GB/s * 0.25 = 48 GLOPS`

Therefore, Computer A is compute-bound at 16 GLOPS.

Computer B:
* Peak compute performance: `4 core * 1 GHz * 8 wide = 32 GLOPS`
* Its bandwidth limit is: `128 GB/s * 0.25 = 32 GLOPS`

Therefore, Computer B can achieve 32 GLOPS.

I would choose Computer B because its wider SIMD allows twice the compute throughput, while its 128 GB/s memory 
bandwidth is sufficient for the fused program.


## PROBLEM 5

### 5.A

Each red-cell update performance 4 additional and 1 multiplication, for a total of 5 FLOPs.

The update read 5 float values from A and writes 1 float value back:

Memory traffic is: `(5 read + 1 write) * 4 byte = 24 byte`

AI is: `5 FLOPs / 24 bytes = 0.208 FLOPs/byte`

### 5.B

For n = 1000 and NN = 100, each tile contains: `100 * 1000 = 100,000 elements`
The tile size is: `100,000 * 4 bytes = 0.4 MB`
LOAD_TILE time: `0.4 MB / 4 MB/s bandwidth = 0.1 seconds`
Only half the cells are red, so STENCIL updates 50,000 cells. Each update performs 5 FLOPS: 
`50,000 * 5 FLOPs = 0.25 MFLOPs`
STENCIL time: `0.25 MFLOSs / 1 MLFOPS bandwidth = 0.25 seconds`
STORE_TILE time: `0.4 MB / 4 MB/s = 0.1 seconds`
Time per tile is: `0.1 + 0.25 + 0.1 = 0.45 seconds`
Therefore are 10 tiles, so: `10 * 0.45 = 4.5 seconds`

### 5.C

From part B, the pipline stage take:

LOAD_TILE = 0.1 s
STENCIL = 0.25 s
STORE_TILE = 0.1 s

The slowest stage is STENCIL, so once the pipline is full, its throughput is one tile every 0.25s.

The first tike required all three stages: `0.1 + 0.25 + 0.1 = 0.45 s.`

The remaining 9 tiles complete at the pipline throughput: `9 * 0.25 = 2.25 s.`

Total time: `0.45 + 2.25 = 2.7 s.`

Therefore, one iteration of the outer loop takes 2.7 seconds. The pipline throughtput is limited by the 0.25 s. STENCIL 
stage.

