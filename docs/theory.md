# RTOS Theory and Design Notes

This document covers the real-time operating system concepts behind the Virtual Network Packet Router simulation. It's written as supplementary material for the project report.

---

## 1. Why an RTOS for Packet Routing?

General-purpose operating systems (like Linux or Windows) use scheduling algorithms optimized for fairness and overall system throughput. That works great for desktop applications, but network routers have a different set of requirements:

- **Bounded latency**: When a packet arrives, it needs to be processed within a known time window. If the router takes too long, upstream buffers overflow and packets start getting dropped.
- **Deterministic behavior**: The system should behave the same way under load as it does when idle. You don't want garbage collection pauses or background updates causing jitter in your packet processing pipeline.
- **Priority handling**: Control-plane traffic (like routing protocol updates) should always get through, even when the data plane is congested with bulk traffic.

An RTOS like QNX Neutrino provides these guarantees through preemptive priority-based scheduling, minimal interrupt latency, and a microkernel architecture that keeps the trusted computing base small and predictable.

---

## 2. QNX Neutrino — The Microkernel Advantage

QNX uses a microkernel design, which means the kernel itself only handles:
- Thread scheduling
- Interrupt handling
- Low-level message passing (IPC)

Everything else — file systems, network stacks, device drivers — runs as user-space processes. If a driver crashes, it doesn't bring down the whole system. This is a big deal for safety-critical applications (QNX is used in cars, medical devices, nuclear power plants, and so on).

For our packet router simulation, the relevant QNX features are:

### 2.1 POSIX Threads (pthreads)

QNX fully supports the POSIX threading model. Each thread has:
- A priority level (0 to 255)
- A scheduling policy (FIFO, round-robin, or sporadic)
- Its own stack and thread-local storage

In our simulation, we use threads to represent input and output ports. Each port runs independently, which mirrors how a real router's network interfaces operate in parallel.

### 2.2 Mutexes and Priority Inheritance

A mutex (mutual exclusion lock) prevents two threads from modifying the same data at the same time. The classic problem with mutexes in real-time systems is **priority inversion**:

> Suppose Thread A (high priority) needs to lock a mutex that Thread C (low priority) currently holds. Thread A blocks and waits. But then Thread B (medium priority) preempts Thread C, so Thread C can't release the lock. Now Thread A — the highest priority thread — is effectively blocked by Thread B, which doesn't even use the mutex. This is called "unbounded priority inversion."

QNX handles this through **priority inheritance**: when a high-priority thread blocks on a mutex held by a lower-priority thread, the lower-priority thread temporarily inherits the higher priority. This ensures the lock holder can finish its critical section without being preempted by medium-priority threads.

In our code, the routing table mutex benefits from this. If a high-priority input port thread needs to look up a route while a lower-priority thread is updating the table, priority inheritance ensures the lookup doesn't get delayed unnecessarily.

### 2.3 Message Passing

QNX's native IPC mechanism is synchronous message passing:
- The sender calls `MsgSend()` and **blocks** until the receiver processes the message and replies.
- The receiver calls `MsgReceive()` to wait for incoming messages.
- After processing, the receiver calls `MsgReply()` to unblock the sender.

This is different from shared-memory approaches because the kernel handles the data transfer (direct copy between address spaces, no intermediate buffer). It's also inherently synchronizing — the sender knows the message was processed because it only unblocks after the reply.

In our simulation, we use a pthread-based queue (which is closer to POSIX message queues) rather than QNX-native `MsgSend`/`MsgReceive`. Both approaches achieve the same goal — safely passing data between threads — but the pthread version is portable to any POSIX system, while the QNX-native version would be more efficient on actual QNX hardware.

### 2.4 Timers

Real-time systems need precise timing. QNX supports POSIX timers (`timer_create`, `timer_settime`) that can deliver signals or pulses at specified intervals.

In our simulation, we use `nanosleep()` to control packet arrival rates. This is a simplified version of what a real system would do with hardware timer interrupts — the NIC triggers an interrupt when a packet arrives, the ISR posts a signal, and a worker thread wakes up to process it.

---

## 3. How the Router Works Internally

Here's the data flow through the simulation:

```
  Input Port 0 ──┐
  Input Port 1 ──┤                              ┌── Output Port 0
  Input Port 2 ──┼── [Routing Table Lookup] ──── [Central Queue] ──┼── Output Port 1
  Input Port 3 ──┘        (mutex)               (mutex + condvar)  ├── Output Port 2
                                                                    └── Output Port 3
```

### 3.1 Packet Generation (Input Ports)

Each input port thread runs a loop:
1. Sleep for a random interval (50–300ms) to simulate variable arrival rates
2. Create a packet with a random destination IP
3. Lock the routing table mutex, look up the destination, unlock
4. If a route was found, push the packet into the central queue
5. If no route exists, increment the drop counter and move on

### 3.2 Routing Table Lookup

The routing table contains four static entries mapping IP subnets to output ports:

| Network | Subnet Mask | Output Port |
|---|---|---|
| 10.0.0.0 | 255.0.0.0 (/8) | Port 0 |
| 172.16.0.0 | 255.240.0.0 (/12) | Port 1 |
| 192.168.1.0 | 255.255.255.0 (/24) | Port 2 |
| 192.168.2.0 | 255.255.255.0 (/24) | Port 3 |

The lookup function iterates through all entries and picks the one with the longest matching prefix (most specific route wins). Every access to the table — whether it's a lookup or an update — goes through the mutex.

### 3.3 Central Packet Queue

The queue is a circular buffer with a fixed capacity of 32 packets. It uses:
- A **mutex** to protect the head/tail pointers and count
- A **"not empty" condition variable** that output threads wait on when the queue is empty
- A **"not full" condition variable** that input threads wait on when the queue is full

This is the textbook producer–consumer pattern. The timed waits ensure no thread blocks forever, which is critical in real-time contexts.

### 3.4 Packet Delivery (Output Ports)

Each output port thread pops packets from the queue and checks the destination port tag. If the packet is meant for this output port, it "delivers" the packet (logs it to the console) and records the latency and statistics. If it's meant for a different port, it puts the packet back.

This re-enqueueing approach isn't ideal from a performance standpoint — a real router would use separate queues per output port. But it works fine at our simulation scale and demonstrates more mutex contention, which makes the RTOS synchronization aspects more visible.

---

## 4. Schedulability and Timing Analysis

For a formal analysis, we can look at the CPU utilization of our system. With the Rate Monotonic Analysis (RMA) framework:

Let `C_i` be the worst-case execution time of task `i`, and `T_i` be its period (minimum inter-arrival time). The system is schedulable if:

```
U = Σ (C_i / T_i) ≤ n × (2^(1/n) − 1)
```

For our 8 threads (4 input + 4 output):
- Input threads: `C ≈ 2ms` processing, `T ≈ 175ms` average arrival interval → `U_i ≈ 0.011`
- Output threads: `C ≈ 1ms` processing, `T ≈ 50ms` (polling interval) → `U_o ≈ 0.020`
- Total: `U ≈ 4 × 0.011 + 4 × 0.020 = 0.124`

The RMA bound for 8 tasks is approximately 0.724. Our utilization of 0.124 is well within that limit, so the system is comfortably schedulable. This makes sense intuitively — we're simulating at human-readable speeds, not processing millions of packets per second.

---

## 5. Comparison: QNX vs Linux for This Workload

| Aspect | QNX Neutrino | Linux (with PREEMPT_RT) |
|---|---|---|
| Kernel model | Microkernel | Monolithic |
| Worst-case interrupt latency | ~1 μs (guaranteed) | ~10–50 μs (best effort) |
| Priority inversion protection | Built into mutex | Optional (PI futex) |
| IPC mechanism | Native message passing | Pipes, sockets, shared memory |
| Certification | Safety-certified (IEC 61508, ISO 26262) | Not certified |
| Ease of development | QNX SDP required | Any GCC toolchain |

For a safety-critical real-world router (like in automotive Ethernet or industrial automation), QNX would be the right choice. For a lab simulation like ours, the functional behavior is the same on both platforms.

---

## 6. Key Takeaways

1. **Threads model hardware parallelism.** Each port running as its own thread is exactly how a multi-port NIC exposes its interfaces to software.

2. **Mutexes are not optional.** Without the routing table mutex, concurrent lookups and updates would corrupt the table. Without the queue mutex, the circular buffer's head/tail pointers would desynchronize.

3. **Condition variables beat polling.** Instead of output threads spinning in a tight loop checking if there's a packet, they block on a condition variable and get woken up only when there's work to do. This saves CPU cycles and reduces power consumption.

4. **Timeouts prevent deadlocks.** If an input thread tries to push into a full queue without a timeout, and the output threads are stuck for some reason, the whole system freezes. Timed waits give us a graceful fallback.

5. **Determinism comes from the scheduler.** The code itself doesn't guarantee real-time behavior — that comes from the RTOS scheduler. Running this on QNX with SCHED_FIFO priorities would give hard real-time guarantees that the same code on Linux cannot provide.
