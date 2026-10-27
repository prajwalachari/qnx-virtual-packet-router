# RTOS Theory Notes

Quick notes on the RTOS concepts behind this project.

---

## Why RTOS?

Normal OSes (Linux, Windows) schedule things for fairness. An RTOS schedules things for **deadlines** — if a packet arrives, it has to be processed within a fixed time. QNX gives us that guarantee through priority-based preemptive scheduling.

## QNX basics

QNX is a **microkernel** — only scheduling, interrupts, and IPC run inside the kernel. Drivers, file systems, networking all run in user space. If a driver crashes, the kernel keeps running.

Relevant features we use:
- POSIX threads with priority levels
- Mutexes with priority inheritance (prevents priority inversion)
- Message passing IPC (`MsgSend`/`MsgReceive` — we use the POSIX queue equivalent in our code for portability)
- POSIX timers

## Key concepts

### Threads
Each network port runs as its own thread. Input ports produce packets, output ports consume them. This is how real NICs work — parallel processing across ports.

### Mutex
Two things need protection:
1. **Routing table** — multiple input threads read it at the same time
2. **Packet queue** — producers and consumers accessing head/tail pointers

Without the mutex you get race conditions and corrupted data.

### Priority inversion
Say a high-priority thread wants the routing table lock, but a low-priority thread holds it. A medium-priority thread could preempt the low one, making the high-priority thread wait indirectly. QNX fixes this with **priority inheritance** — the lock holder temporarily gets boosted.

### Producer-consumer pattern
Input threads push packets, output threads pop them. When the queue is empty, output threads block on a condition variable instead of busy-waiting. When full, input threads block too. `pthread_cond_timedwait` adds a timeout so nothing hangs forever.

### Timers
We use `nanosleep()` with random delays (50-300ms) to simulate packets arriving at different rates. In a real system this would be hardware interrupts from the NIC.

## Routing

The table has 4 entries mapping IP subnets to ports. Lookup uses longest-prefix matching — most specific match wins. Same idea as a real router's forwarding table.

## Throughput

Measured as (packets successfully routed) / (total simulation time). Printed at the end along with per-port stats and drop count.

## Schedulability

Using Rate Monotonic Analysis — if total CPU utilization stays under `n(2^(1/n) - 1)` for n tasks, all deadlines are met. Our utilization is well under 1% so we're fine, but that's because we're simulating at slow speeds for readability.

## References

- QNX docs: https://www.qnx.com/developers/docs/
- Silberschatz, *Operating System Concepts*
- POSIX threads tutorial: https://hpc-tutorials.llnl.gov/posix/
