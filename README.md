# mm_slowpath_stressor
This project contains a set of program tools created with purpose to push on the Linux's allocation slowpath and measure overhead of those slowpath algorithms.

## Tools:
- *stressor.c*
  
  This program forks processes that allocate anonymous or pagecache memory and then touch each page while doing some arithmetic work in-between.
  The aim is to measure how much overhead penalty (in terms of execution time) is added by the kernel's allocation slowpath.


- *balloon.c*
  
  This program is meant to simply set itself to specified oom_score_adj (ideally -1000) and allocate+fault amount of memory, which can be used to quickly shrink the available memory pool.
 

## Compilation
Build with simple `gcc` with no extra arguments (tested on RHEL8.7 and RHEL9.1).
