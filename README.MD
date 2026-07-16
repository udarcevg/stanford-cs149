

## Notes

### Assignments 1
#### 1
Multithreading parallelizes work across multiple CPU cores or hardware threads, while SIMS
parallelizes operations on multiple data elements using vector register in a single instruction.

* Multithreading = many workers
* SIMD = one worker with many hands


#### 2

##### 2
I ran ./myexpr -s  10000 with vectors widths 2, 4, 8 and 16. As the vector width increased, vector
utilization decreased. This happens because wider vectors have more lanes, but some lines  become inactive due
to masking, especially in the final partial vector iteration or in conditional code. With a large vector
width, the same amount of leftover work can waste more SIMD lanes, reducing overall utilization.
