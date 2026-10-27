# Virtual Network Packet Router (QNX RTOS)

Simulates a packet router where packets arrive from multiple input ports and get forwarded to output ports based on a routing table. Built using POSIX threads and mutexes — runs on QNX Neutrino or any Linux system.

Done as part of RTOS coursework.

## How it works

- 4 input port threads generate packets with random destination IPs
- Routing table does longest-prefix match to find the output port
- Packets go into a shared queue (mutex + condition variables)
- 4 output port threads pick up and deliver packets meant for them
- Stats printed at the end — throughput, drops, per-port counts

## RTOS concepts used

- **Threads** — each port is a separate pthread
- **Mutex** — protects routing table and packet queue
- **Condition variables** — blocking queue (producer-consumer)
- **Timers** — `nanosleep()` for random packet arrival delays

## Build

```bash
# linux
make
./packet_router

# qnx
make qnx
```

## Files

```
src/main.c            - entry point, thread creation
src/router.c          - queue, input/output port logic
src/routing_table.c   - routing table with mutex
include/              - header files
test/test_routing.c   - basic tests for route lookup
docs/theory.md        - short writeup on rtos theory
```

## Default routes

| Network | Mask | Port |
|---|---|---|
| 10.0.0.0 | /8 | 0 |
| 172.16.0.0 | /12 | 1 |
| 192.168.1.0 | /24 | 2 |
| 192.168.2.0 | /24 | 3 |

Packets to unrecognized IPs (like 8.8.8.8) get dropped.

## Sample output

```
[Input Port 0] PKT #001 -> dest 10.0.0.42 -> routed to Port 0
[Input Port 1] PKT #003 -> dest 8.8.8.8 : NO ROUTE (dropped)
[Output Port 0] DELIVERED PKT #001 | 192.168.0.1 -> 10.0.0.42 | pri=1 | latency=12.3ms

========================================================
              ROUTER PERFORMANCE REPORT
========================================================
  Packets Received      : 50
  Packets Routed        : 40
  Packets Dropped       : 10
  Throughput            : 8.85 packets/sec
========================================================
```

## License

MIT
