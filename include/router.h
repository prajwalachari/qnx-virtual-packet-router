/*
 * router.h
 * --------
 * Core router module declarations.
 * Manages input port threads, output port threads,
 * the central packet queue, and throughput statistics.
 *
 * Author : Prajwal
 * Course : Real-Time Operating Systems (RTOS)
 * Project: Virtual Network Packet Router Simulation (QNX)
 */

#ifndef ROUTER_H
#define ROUTER_H

#include <pthread.h>
#include "packet.h"
#include "routing_table.h"

#define PACKET_QUEUE_SIZE  32
#define TOTAL_PACKETS      50   /* total packets to simulate per input port */

/* Thread-safe circular queue for packets waiting to be routed */
typedef struct {
    Packet          buffer[PACKET_QUEUE_SIZE];
    int             head;
    int             tail;
    int             count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} PacketQueue;

/* Statistics tracked during the simulation run */
typedef struct {
    int             total_received;
    int             total_routed;
    int             total_dropped;       /* packets with no matching route */
    int             per_port_sent[MAX_PORTS];
    int             per_port_received[MAX_PORTS];
    struct timespec start_time;
    struct timespec end_time;
    pthread_mutex_t lock;
} RouterStats;

/* Arguments passed to each input port thread */
typedef struct {
    int             port_id;
    int             num_packets;         /* how many packets this port generates */
    RoutingTable   *rt;
    PacketQueue    *queue;
    RouterStats    *stats;
    int            *running;             /* pointer to shared running flag */
} InputPortArgs;

/* Arguments passed to each output port thread */
typedef struct {
    int             port_id;
    PacketQueue    *queue;
    RouterStats    *stats;
    int            *running;
} OutputPortArgs;

/* ---- Queue operations ---- */
void  packet_queue_init(PacketQueue *q);
int   packet_queue_push(PacketQueue *q, Packet *pkt, int timeout_ms);
int   packet_queue_pop(PacketQueue *q, Packet *out, int timeout_ms);
void  packet_queue_destroy(PacketQueue *q);

/* ---- Stats operations ---- */
void  router_stats_init(RouterStats *s);
void  router_stats_print(RouterStats *s);
void  router_stats_destroy(RouterStats *s);

/* ---- Thread entry points ---- */
void *input_port_thread(void *arg);
void *output_port_thread(void *arg);

#endif /* ROUTER_H */
