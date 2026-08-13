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

