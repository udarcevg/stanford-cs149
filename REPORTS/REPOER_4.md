## Problem 1

| Instruction  |     P0 X |     P1 X |     P0 Y |     P1 Y |
| ------------ | -------: | -------: | -------: | -------: |
| `P0 LOAD X`  | S [MISS] |        I |        I |        I |
| `P0 LOAD Y`  |        S |        I | S [MISS] |        I |
| `P1 LOAD Y`  |        S |        I |        S | S [MISS] |
| `P1 LOAD Y`  |        S |        I |        S |  S [HIT] |
| `P1 STORE Y` |        S |        I |        I |  M [HIT] |
| `P0 STORE Y` |        S |        I | M [MISS] |        I |
| `P0 LOAD X`  |  S [HIT] |        I |        M |        I |
| `P1 STORE X` |        I | M [MISS] |        M |        I |
| `P0 LOAD X`  | S [MISS] |        S |        M |        I |


## Problem 2

### 2.A

The key difference is that a read-write lock explicitly track readers using shared metadata such as mum_readers, 
and a writer waits untill all readers release the lock. MSI instead distributes cohereance state across cache and
allow a writter to actively invalidate shared copies.

Using read-write locks for cache coherence would be difficult because ordinary memory reads do not have a natural
 read_unlock point, so it is unclear when cache should  stop being counted as a reader. In additional, maintaining 
a shared reader count would require frequent atomic updates to common metadata, creating serialization and large 
amount of cohereance traffic even for read-oonly accesess.


### 2.B

| P0 | P1 | P2 | Bus transactions    | Cache states     | Data comes from |
| -- | -- | -- | ------------------- | ---------------- | --------------- |
| LL | LL | LL | P0:RD, P1:RD, P2:RD | P0:S, P1:S, P2:S | Memory          |
| SC | SC | SC | P0:RDX              | P0:M, P1:I, P2:I | N/A             |
|    | LL | LL | P1:RD, P2:RD, P0:WB | P0:S, P1:S, P2:S | P0              |
|    | SC | SC | P1:RDX              | P0:I, P1:M, P2:I | N/A             |
|    |    | LL | P2:RD, P1:WB        | P0:I, P1:S, P2:S | P1              |
|    |    | SC | P2:RDX              | P0:I, P1:I, P2:M | N/A             |

At each step, the lowest-numbered completing processor wins the conflicting BusRdx. 
Its successful write invalidates the other processors cache lines and LL reservations,
causing their SC operations to fail and retry. The successful order is P0, then P1, then P2.


## Problem 3

### 3.A

The processor holding the lock does not necessarily hold the cache line containing lock in the M state. While
the lock holder is executing  its critical section, other processors continue spinning and executing CAS operations.
Since every CAS is treated as a write by the coherence protocol, even an unsuccessful CAS must obtain exclusive 
ownership of the cache line. Therefore, a waiting processor can move the line into its M state and invalidate the lock 
holder's copy even thought the lock value remains 1 and the original theread still logically holds the lock.


### 3.B

Since W-W ordering is relaxed, thread 1's write to `l` during `unlock` may become visible before its earlier write to 
`x`. Thread 2 can therefore observe `l = 0`, successfully acquire the lock, while still observing the old value `x = 0`.
In addition, because W-R ordering is relaxed, theread 2's read of `x` may be reordered before its lock-acquiring CAS 
is globlky completed. Thus, cache coherence alone does not provide the ordering betwenn accesses to differnt memory 
locations required for correct lock semantics. Acquire/release ordering is needed.


## Problem 4

### 4.A

Yes. `insert_head(8)` insert between node 0 and 10, while `insert_tail(27)` insert between nodes 25 and 30. Therefore, 
the two threads modify different next and pre pointers and their write do not inrefere. Any inteleaving of the two 
insetions produces the correct final list: `0-8-10-25-27-30-75`


### 4.B

No. Both threads can observe the original gap between noded 25 and 30 and attempt to insert it simultaneously. They 
then race while writing 25->next and 30->prev. For example, one interleaving can leave 25->next = 27 while 30->prev = 26,
so forward traversal sees node 27 but backward traversal sees node 26. Thus, the doubly linked list becomes inconsistent
 because the two insertions update the same links without synchronizations.


### 4.C

A deadlock can occur because the two threads traverse the list in opposite directions. T1 can hold the lock on node 30 
while waiting to acquire the lock on node 30, while T2 simultaneously holds the lock on node 30 while waiting to 
acquire the lock on node 25. Neither thread can release its current lock because hand-over-hand traversal requires 
acquiring the next lock first. Thus, threads wait for each other indefinitely.

### 4.D

```c++
lock(cur);

while (true) {
    Node *next = cur->next;
    
    if (trylock(next)) {
        if (value > cur->value && value <= next->value) {
            n->prev = cur;
            n->next = next;
            next->prev = n;
            prev->next = n;
            unlock(next);
            unlock(cur);
            return;
        }
        unlock(cur);
        cur = next;
    } else {
        unlock(cur);
        lock(cur);
    }
}
```

Use `trylock()` when attempting to acquire the next node during hand-over-hand traversal. If it succeeds, hold both the 
current and next node locks, check whether they surround the insertion position, and either insert or release the 
previous lock and continue.

If `trylock()` fails, release the current lock instead of blocking while holding it. Later reacquire the current node 
and re-read its `next` pointer before continuing, since another thread may have inserted nodes while no lock were held. 
Because the list has no deletions, the current node itself cannot disappear, so the traversal does not need to restart 
from the head. This breaks the circular-wait conditions that caused the deadlock while still ensuring that both 
neighboring nodes are locked during insertions. 


## PRACTICE PROBLEM 1

| Operation    | P0 X     | P1 X     | P0 Y     | P1 Y     |
| ------------ | -------- | -------- | -------- | -------- |
| `P0 LOAD X`  | S [MISS] | I        | I        | I        |
| `P0 LOAD X`  | S [HIT]  | I        | I        | I        |
| `P1 STORE Y` | S        | I        | I        | M [MISS] |
| `P1 STORE X` | I        | M [MISS] | I        | M        |
| `P1 LOAD Y`  | I        | M        | I        | M [HIT]  |
| `P1 STORE Y` | I        | M        | I        | M [HIT]  |
| `P1 LOAD Y`  | I        | M        | I        | M [HIT]  |
| `P0 STORE X` | M [MISS] | I        | I        | M        |
| `P0 STORE Y` | M        | I        | M [MISS] | I        |


## PRACTICE PROBLEM 2

### 2.A
The poor scaling is caused by false sharing.  `position` and `angriness` for multiple students occupy the same 
64-byte cache lines. Although the threads modify different fields, writes required exclusive ownership of the entire 
cache line, causing the lines to repeatedly move between the cores.

### 2.B

Change the representation from Array of Structures to a Structure of Arrays:

float positions[N];
float angriness[N];

Thread 0 updates only `positions`, while thread 1 updates only `angriness`. Their writes then occur on separate cache 
lines, eliminating the false sharing without substantially increasing memory usage. 


## PRACTICE PROBLEM 3

### 3.A

With `NUM_SONGS=8`, each partial_counts row in only 8*4=32 bytes. Because partial_count is 64-byte aligned, 
partial_counts[0] and partial_counts[1] occupy the same 64-byte cache line. The two threads therefore cause false 
sharing as they repeatedly write different counters is the same line, causing the line to ping-pong between the cores.

With NUM_SONGS=16, each row is exactly 64 bytes, so the two rows occupy different cache lines. Each thread can update 
its own cache line without invalidating the other threads line, so Olivia's approach should provide much better, 
near-linear scaling.

### 3.B

The original program does not scale well because after the barrier only thread 0 performs the reduction over all 
NUM_SLIDES=2000  counters, while thread 1 is idle. Since N=5000, this serial reduction is a significant fraction of 
the total work.

Parallelize the reduction by splitting the output counter between the threads:

```c++
int votes[N];
int counts[NUM_SLIDES];
int partial_counts[2][NUM_SLIDES];
// T0
for (i = 0; i < N / 2; i++) {
    if (votes[i] < NUM_SLIDES    {
        partial_counts[0][votes[i]]++
    })
}
barrier();
for (int i = 0; i < N / 2; i++) {
    counts[i] = partial_counts[0][i] + partial_counts[1][i];
}
// T1
for (i = N / 2; i < N ; i++) {
    if (votes[i] < NUM_SLIDES    {
        partial_counts[0][votes[i]]++
    })
}
barrier();
for (i = N / 2; i < N ; i++) {
    counts[i] = partial_counts[0][i] + partial_counts[1][i];
}
```


## PRACTICE PROBLEM 4

### 4.A

With 4-bite cache lines, counter[0] and counter[1] occupy different cache lines, so there is no false sharing.

For each thread, the first load causes I->S and cost 1 + 10 = 11 cycles. The following store causes S->M and also 
costs 11 cycles. Therefore, the first iterations costs 22 cycles.

The line then remains in M because the other threads accesses a different cache line. Each subsequent iteration has 
a 1-cycle load hit and a 1-cycle store hit, for 2 cycles.

`Total = 2 * NUM_ITERS + 20`

Transition per thread: I->S = 1, S->M = 1
No coherence-induced invalidations or writebacks.


### 4.B

With 8-byte cache lines, counter[0] and counter[1] occupy the same cache line, so the threads suffer from false sharing.

The first iterations by a thread costs:
I->S: PrRd + BusRd = 11 cycles
S->: PrWr + BusRdX = 11 cycles
Total = 22 cycles

After that, the other core always owns the shared line in M. Therefore each new  iterations requires:
I->S with remove M->S and BusWB = 21 cycles
S->M with BusRdX = 11 cycles
Total = 32 cycles

Thus, for one thread: 22 + 32(NUM_ITERS - 1) = 32*NUM_ITERS - 10 cycles.

Per thread transitions:
I->S: NUM_ITERS
S->M: NUM_ITERS
M->S: NUM_ITERS - 1
S->I: NUM_ITERS - 1

## PRACTICE PROBLEM 5


### 5.A.

A single ISPC gang is mapped to SIMD execution resources on one CPU core. Therefore, all programm instances in the gang 
access `result` through the same core's cache rather than through separate private caches on different cores. As a 
resuls, there are no copies of `result` distributed across different cores that need to be kept coherent, so cache 
coherance is not particulary relevant in this setup.

### 5.B

Each 32-byte cache line contains 8 floats. With interleaved assignment, all four cores write element of every cache 
line, causing false sharing.

For each of the 4 cache lines, the first write causes I->M and costs PrWr + BudRdX = 11 cycles. Each of the next 7 
writes transfers ownership from a core holding M to another core, requiring BusRdX + BusWB and costing 21 cycles.

Total = 4 * (11 + 7 * 21)  =632 cycles

Transitions: I->M: 32, M->I: 28, I->S: 0, S->M: 0, M->S: 0


### 5.C

With blocked assignments, each program instance writes exactly one 32-byte cache line:
``
Core0: result[0..7]
Core1: result[8..15]
Core2: result[16..23]
Core3: result[24..31]
``

The first write by each core causes I->M and cost 11 cycles. The remaining 7 writes to that line are M-state hits and 
cost 1 cycles each.

`Total = 4 core (11 first + 7 subsequent one) = 72 cycles`

Transitions: I->M: 4

Blocked assignments avoid the false sharing and cache line ping-pong seed in the interleaved version.

## PRACTICE PROBLEM 6

### 6.A

Test 3 can deadlock. `insert_head(13)` traverses from left to right, while `insert_tail(4)` traverses from right to left.
 With hand-over-hand locking, one thread can hold the lock on node 6 while waiting for node 7, while the other holds 
the lock in node 7 while waiting for node 6. This creates a circular

### 6.B

Use `trylock()` when attempting to acquire the second node lock. If the second lock cannot be acquired, do not block 
while holding the first lock; release it so that the conflicting thread can make progress.

To avoid livelock, male the behavior asymmetric. For example, give `insert_head` priority; when an `insert_tail` 
traversal fails `trylock()`, it release its current lock and yields, allowing the `insert_head` traversal to pass. 
It can then reacquire its position and continue, rechecking the adjacent pointer because the list may have change 
while it held no locks.

This eliminates circular wait and prevent both threads from repeatedly releasing and reacquiring their lock in 
synchrony. Starvation is possible, which the problem permits.


## PRACTICE PROBLEM 7

`atomicMin2` has a larger performance advantage for the second initialization, `values[i] = i`.

With decreasing values, most values are new minima, so both `atomicMin1` and `atomicMin2` must execute CAS operations 
frequently.

With increasing values, once a small global minimum has been established, most later values cannot improve it. 
`atomicMin2` detects new == old and return without executing CAS, while `atomicMin1` still execute CAS even when 
the value does not change. Since CAS is treated as a write by the MSI coherence protocol, `atomicMin1` causes 
unnecessary exclusive ownership transfer and cache-line ping-pong. Therefore `atomicMin2` performs much better for the 
increasing input.


## PRACTICE PROBLEM 8

### 8.A

Circle: No

Deadlock cannot occur because each thread holds at most one pixel lock at a time. A thread locks a pixel, updates it, 
and releases the lock before attempting to acquire the lock for the next pixel. Threads may contend for the same pixel 
lock and temporarily wait, but no thread waits for another lock while already holding one. Therefore, a circular wait 
cannot form.


### 8.B

LOACK_A is likely to be faster for this workload because contention on any individual pixel lock is very low. The image 
contains millions if pixel, only four thread are running, and line locations are random, so most lock acquisitions are 
uncontented.

In the uncontended case, LOCK_A performs a single CAS and succeed. LOCK_B first reads the lock and then performs CAS, 
adding an extra memory operations. LOCK_B is more useful under high contention because read spinning reduces repeated 
CAS coherence traffic, but that benefit is unlikely to matter much this workload.

### 8.C

Circle: Yes

Deadlock is possible because a thread now holds one pixel lock while truing to acquire a second. Two threads traversing 
neighboring pixel in opposite directions can produce: 

* T0 holds A and wait for B
* T1 holds B and wait for A

Ti eliminate deadlock, impose a global ordering on pixel locks. For example, assign each pixel unique linear index and 
always acquire the lower-index pixel lock before the higher-indexed one, regardless of traversal directions. This 
preserves fine-grained locking while preventing circular wait.


### 8.D

```c++
void renderLine(int xstart, int ysrtart, int xend, int yend) {
    int xcur = xstart;
    int ycur = ystart;
    
    while (xcur != xend && ycur !+ yend) {
        int xnext, ynext;
        nextPixel(xcur, ycur, &xnext, &ynext);
        
        atomic {
            if (fabs(pixels[xcur][ycur] - pixels[xnext][ynext] < .5) {
                pixels[xcur][ycur] -= 0.1f;
            } else {
                pixels[xcur][ycur] += 0.1f;
            }
        }
        
        xcur = xnext;
        ycur = ynext;
    }
}
```


## PRACTICE PROBLEM 9

### 9.A

The barrier ensures that every thread has completed all of its simulations and updates to `lower_so_far` before thread 
0 prints the result. Without the barrier, thread 0 could finish early abd print the current minimum while another 
thread still  running and may later discover a lower value. `atomicCAS` makes individual updates atomic, but it does 
not guarantee that alk threads have finished before result is consumed. 

### 9.B

The program should achieve near-perfect speedup because `simulate_random_kick()` is extremely expensive and all call 
take about the same amount of time. Most execution time is spent doing independent computation, while the atomicCAS 
operations on `lowest_so_far` persent only a tiny fraction of the total runtime, so cache-coherence contention should 
have little impact.

A possible optimization is for each thread to maintain a private local minimum and perform only one final atomic update
(or a reduction) after its simulations. However, this is unlikely to improve performance substantially in this case 
because synchronization overhead is already negligible compared with the simulation cost.

### 9.C

```c++
int local_min = VERY_LARGE;
float ball_height;

for (int i = 0; i < 1000000 / num_threads(); i++) {
    simulate_random_kick(&ball_height);
    local_min = min(local_min, (int)ball_height);
}

atomicMin(&lowest_so_far, local_min);

barrier();

if (get_thread_id() == 0)
    printf("The lowest height is %d cm\n", lowest_so_far);
```

The original implementation is unlikely to achieve near-perfect speedup because the simulation is cheap while every 
iteration performs an atomic operation on the same shared cache line. On a high-core-count processor this creates 
heavy coherence traffic, cache-line ping-pong, and failed CAS retries. Keeping a private minimum per thread and 
combining the minima only at the end greatly reduces synchronization and coherence traffic.


### 9.D

The correctness problem is that `lowest_so_far` and `lowes_vid` are not updated atomically as a pair. A thread can 
successfully update `lower_so_far`, then another thread can install an even lower value and its video, after which the 
first thread can overwrite `lowest_vid` with the video for its larger height.

For example, one thread may set the minimum to 50 and another later set it to 40, but the first thread can still 
write the video for 50 last. The final height and video would then be inconsistent. The barrier does not fix this, 
because it only waits for all threads to finish.


### 9.E

```c++
for (int i = 0; i < 1000000 / num_thread(); i++) {
    simulate_random_kick(&ball_height, &vid);
    
    lock(&min_lock);
    
    if (ball_height < lowest_so_far) {
        lowest_so_far = ball_height;
        lowest_vid = vid;
    }
    unlock(&min_lock)
}
```

The lock protects the comparison and hte updates to both `lowest_so_far` and `lowest_vid`, ensuring that the stored 
video always corresponds to the stored minimum. The expensive simulations remaind outside the critical section to 
maximize concurrency.

## PRACTICE PROBLEM 10

Graph A: Deadlock is possible.
T0 can hold node 0 while waiting for node 1, while T1 holds node 1 while waiting for node 0.

Graph B: Deadlock is also possible.
Again, T0 can hold node 0 and eventually wait for node 1, while T1 holds node 1 and waits for node 0.

The underlying problem is inconsistent lock acquisition order. A standard fix is to acquire all required node locks in 
a global order, such as in increasing node-ID order.


## PRACTICE PROBLEM 11

### 11.A

The code can deadlock because the two threads acquire the same locks in different orders. For example, T0 can hold l1 
and l2 while waiting for l3, while T1 holds l3 and waits for l2. Neither thread can proceed.

Fix the problem by defining a global lock order, such as l1 < l2 < l3, and requiring every thread  to acquire the locks 
in that order. Both threads should lock l1, then l2, then l3 before entering the critical section. This preserves 
mutual exclusion while eliminating circular wait.

### 11.B

```c++
struct HashTable {
    Node* bins[NUM_BINS];
    Lock locks[NUM_BINS];
};

bool tableInsert(HashTable* table, string s, string s2) {
    int idx1 = hashFunction(s1);
    int idx2 = hashFunction(s2);
    
    int first = min(idx1, idx2);
    int second = max(idx1, idx2);
    
    lock(&table->locks[first]);
    if (first != second) {
        lock(&table->locks[second]);
    }
    
    bool result = false;
    
    if (!findInList(table->bins[idx1], s1) &&
        !findInList(table->bins[idx2], s2)
        ) {
        insertList(table->bins[idx1], s1);
        insertList(table->bins[idx2], s2);
        result = true;
    }
    
    if (first != second) {
        unlock(&table->locks[second]);
    }
    unlock(&table->locks[first]);
    return result
}
```

Using ine lock per bin allows operations on unrelated bins to execute concurrently, Acquiring the two required locks in 
increasing bin-index order prevents deadlock, and the `idx1 == idx2` case acquire the bin lock only once. 