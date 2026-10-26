# Virtual Network Packet Router — QNX RTOS Simulation

A multithreaded packet routing simulator built on top of QNX Neutrino RTOS concepts. This project was developed as part of my coursework on Real-Time Operating Systems.

The program simulates a basic network router where packets come in from multiple input ports (each running on its own thread), get matched against a routing table, and are forwarded to the right output port. It uses standard RTOS primitives — threads, mutexes, condition variables, and timed delays — to handle everything concurrently and safely.

---

## What Does It Actually Do?

Think of it like a simplified version of what happens inside a physical network switch:

1. **Four input ports** run as separate threads. Each one generates packets with random destination IPs at slightly different rates (to make it feel more like real bursty network traffic).

2. A **shared routing table** maps destination subnets to output ports. The lookup uses longest-prefix matching, which is how actual routers figure out where to send stuff.

3. Packets that match a route get pushed into a **central queue** (a circular buffer protected by a mutex and condition variables). Packets with no matching route are silently dropped.

4. **Four output port threads** continuously pull from the queue. Each one only cares about packets tagged for its port number, delivering them and logging the result.

5. At the end, the program prints a summary — total packets processed, how many got dropped, throughput in packets per second, and a per-port breakdown.

---

## RTOS Concepts Used

| Concept | Where It Shows Up |
|---|---|
| **POSIX Threads (`pthread`)** | Each input and output port runs in its own thread |
| **Mutex (`pthread_mutex_t`)** | Protects the routing table and the packet queue from race conditions |
| **Condition Variables** | Used for blocking push/pop on the queue (producer–consumer pattern) |
| **Timed Waits** | `nanosleep()` simulates variable packet arrival rates; `pthread_cond_timedwait()` prevents threads from hanging forever |
| **Shared Data Structures** | The routing table and statistics counters are shared across threads |

The code is written using POSIX APIs, which means it compiles and runs on Linux too — but it's designed with QNX in mind. On an actual QNX target, you'd get hard real-time scheduling guarantees that Linux can't provide.

---

## Project Layout

```
qnx-virtual-packet-router/
├── include/
│   ├── packet.h            # Packet data structure
│   ├── routing_table.h     # Routing table interface
│   └── router.h            # Queue, stats, thread declarations
├── src/
│   ├── main.c              # Entry point, thread orchestration
│   ├── router.c            # Queue implementation, input/output threads
│   └── routing_table.c     # Routing table with mutex + longest-prefix match
├── docs/
│   └── theory.md           # RTOS theory and design notes
├── test/
│   └── test_routing.c      # Basic routing table unit test
├── Makefile                 # Build for GCC or QNX qcc
├── .gitignore
├── LICENSE
└── README.md               # You're reading this
```

---

## Building

### On Linux (for testing)

```bash
make
./packet_router
```

Requires GCC and pthreads (which pretty much every Linux distro has out of the box).

### On QNX

If you have the QNX Software Development Platform set up:

```bash
make qnx
# then copy packet_router_qnx to your QNX target and run it
```

---

## Sample Output

```
============================================================
    Virtual Network Packet Router Simulation (QNX RTOS)
    RTOS Concepts: Threads, Mutex, Message Passing, Timers
============================================================

[Main] Routing table initialized with default routes:

+------+------------------+------------------+--------+
| Idx  | Network          | Mask             | Port   |
+------+------------------+------------------+--------+
| 0    | 10.0.0.0         | 255.0.0.0        | 0      |
| 1    | 172.16.0.0       | 255.240.0.0      | 1      |
| 2    | 192.168.1.0      | 255.255.255.0    | 2      |
| 3    | 192.168.2.0      | 255.255.255.0    | 3      |
+------+------------------+------------------+--------+

[Input Port 0] PKT #001 -> dest 10.0.0.42 -> routed to Port 0
[Input Port 2] PKT #002 -> dest 192.168.2.117 -> routed to Port 3
[Output Port 0] DELIVERED PKT #001 | 192.168.0.1 -> 10.0.0.42 | pri=1 | latency=12.3ms
[Input Port 1] PKT #003 -> dest 8.8.8.8 : NO ROUTE (dropped)
...

========================================================
              ROUTER PERFORMANCE REPORT
========================================================
  Simulation Duration   : 4.521 seconds
  Packets Received      : 50
  Packets Routed        : 40
  Packets Dropped       : 10
  Throughput            : 8.85 packets/sec

  Per-Port Breakdown:
  +---------+----------+----------+
  | Port    | Ingress  | Egress   |
  +---------+----------+----------+
  | Port 0  | 12       | 9        |
  | Port 1  | 13       | 11       |
  | Port 2  | 12       | 10       |
  | Port 3  | 13       | 10       |
  +---------+----------+----------+
========================================================
```

---

## Design Decisions

- **Single shared queue vs per-port queues:** I went with one central queue for simplicity. A production router would use per-output-port queues to avoid the "wrong port, put it back" issue. But for demonstrating mutex contention and thread synchronization, one shared queue actually makes the RTOS concepts more visible.

- **Longest-prefix matching:** Even though we only have four routes, I implemented proper prefix matching anyway because that's how real routing works. It makes the routing table module reusable if someone wanted to extend this with more complex topologies.

- **Random arrival delays:** Packets don't arrive at a fixed rate — the delay is randomized between 50–300ms per packet. This creates more interesting contention patterns and makes the throughput measurements more realistic than a perfectly uniform stream would.

- **Timed queue operations:** Both push and pop use `pthread_cond_timedwait` with a timeout so threads never get stuck indefinitely. This is an important pattern in RTOS programming where you can't afford a deadlocked thread consuming resources.

---

## Extending This

Some things I thought about but didn't implement (could be good for anyone who wants to take this further):

- **Per-output-port queues** — would remove the need for output threads to re-enqueue packets meant for other ports
- **Priority-based scheduling** — right now all threads run at default priority; on QNX you could use `SCHED_FIFO` to give high-priority packet threads preemption guarantees
- **QNX native message passing** — replacing the pthread-based queue with `MsgSend`/`MsgReceive` channels would make this fully idiomatic QNX, at the cost of portability
- **Dynamic route updates** — add a control thread that modifies the routing table while traffic is flowing, to stress-test the mutex protection

---

## References

- QNX Neutrino RTOS documentation — [qnx.com](https://www.qnx.com/developers/docs/)
- POSIX Threads Programming — [computing.llnl.gov](https://hpc-tutorials.llnl.gov/posix/)
- Tanenbaum, *Modern Operating Systems* (scheduling and synchronization chapters)
- Silberschatz, Galvin & Gagne, *Operating System Concepts*

---

## License

MIT License. See [LICENSE](LICENSE) for details.
