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