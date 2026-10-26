/*
 * router.c
 * --------
 * Core simulation engine for the virtual packet router.
 *
 * This file implements:
 *  - Thread-safe packet queue (circular buffer with mutex + condvars)
 *  - Input port threads (simulate packet arrival using timers)
 *  - Output port threads (consume routed packets)
 *  - Statistics tracking and reporting
 *
 * The design mirrors how a real QNX-based network device would
 * separate ingress and egress processing into independent threads,
 * communicating through protected shared data structures.
 *
 * Author : Prajwal
 * Course : Real-Time Operating Systems (RTOS)
 * Project: Virtual Network Packet Router Simulation (QNX)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "router.h"

/* ================================================================
 *  PACKET QUEUE IMPLEMENTATION
 *  Circular buffer with blocking push/pop and timed waits.
 * ================================================================ */

void packet_queue_init(PacketQueue *q)
{
    q->head  = 0;
    q->tail  = 0;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

/*
 * Push a packet into the queue.
 * Blocks for up to timeout_ms milliseconds if the queue is full.
 * Returns 0 on success, -1 on timeout or error.
 */
int packet_queue_push(PacketQueue *q, Packet *pkt, int timeout_ms)
{
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += (long)timeout_ms * 1000000L;
    /* handle nanosecond overflow */
    while (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&q->lock);

    while (q->count >= PACKET_QUEUE_SIZE) {
        int rc = pthread_cond_timedwait(&q->not_full, &q->lock, &deadline);
        if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&q->lock);
            return -1;  /* queue stayed full, couldn't enqueue */
        }
    }

    q->buffer[q->tail] = *pkt;
    q->tail = (q->tail + 1) % PACKET_QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

/*
 * Pop a packet from the queue.
 * Blocks for up to timeout_ms milliseconds if the queue is empty.
 * Returns 0 on success, -1 on timeout.
 */
int packet_queue_pop(PacketQueue *q, Packet *out, int timeout_ms)
{
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += (long)timeout_ms * 1000000L;
    while (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&q->lock);

    while (q->count == 0) {
        int rc = pthread_cond_timedwait(&q->not_empty, &q->lock, &deadline);
        if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
    }

    *out = q->buffer[q->head];
    q->head = (q->head + 1) % PACKET_QUEUE_SIZE;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

void packet_queue_destroy(PacketQueue *q)
{
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

/* ================================================================
 *  ROUTER STATISTICS
 * ================================================================ */

void router_stats_init(RouterStats *s)
{
    memset(s, 0, sizeof(RouterStats));
    pthread_mutex_init(&s->lock, NULL);
    clock_gettime(CLOCK_MONOTONIC, &s->start_time);
}

void router_stats_print(RouterStats *s)
{
    pthread_mutex_lock(&s->lock);
    clock_gettime(CLOCK_MONOTONIC, &s->end_time);

    double elapsed = (s->end_time.tv_sec - s->start_time.tv_sec)
                   + (s->end_time.tv_nsec - s->start_time.tv_nsec) / 1e9;

    printf("\n");
    printf("========================================================\n");
    printf("              ROUTER PERFORMANCE REPORT                 \n");
    printf("========================================================\n");
    printf("  Simulation Duration   : %.3f seconds\n", elapsed);
    printf("  Packets Received      : %d\n", s->total_received);
    printf("  Packets Routed        : %d\n", s->total_routed);
    printf("  Packets Dropped       : %d\n", s->total_dropped);

    if (elapsed > 0.0) {
        printf("  Throughput            : %.2f packets/sec\n",
               s->total_routed / elapsed);
    }

    printf("\n  Per-Port Breakdown:\n");
    printf("  +---------+----------+----------+\n");
    printf("  | Port    | Ingress  | Egress   |\n");
    printf("  +---------+----------+----------+\n");
    for (int i = 0; i < MAX_PORTS; i++) {
        printf("  | Port %-2d | %-8d | %-8d |\n",
               i, s->per_port_sent[i], s->per_port_received[i]);
    }
    printf("  +---------+----------+----------+\n");
    printf("========================================================\n\n");

    pthread_mutex_unlock(&s->lock);
}

void router_stats_destroy(RouterStats *s)
{
    pthread_mutex_destroy(&s->lock);
}

/* ================================================================
 *  INPUT PORT THREAD
 *
 *  Each input port thread simulates a network interface receiving
 *  packets at variable intervals (mimicking real traffic bursts).
 *  It generates packets with random destination addresses, performs
 *  a routing table lookup to tag the correct output port, and
 *  pushes the packet into the central queue.
 * ================================================================ */

/*
 * Generate a random destination IP that maps to one of our
 * four configured subnets (or occasionally an unroutable address).
 */
static uint32_t random_dest_addr(void)
{
    int choice = rand() % 5;
    switch (choice) {
        case 0: return 0x0A000001 + (rand() % 254);     /* 10.0.0.x       */
        case 1: return 0xAC100001 + (rand() % 254);     /* 172.16.0.x     */
        case 2: return 0xC0A80101 + (rand() % 254);     /* 192.168.1.x    */
        case 3: return 0xC0A80201 + (rand() % 254);     /* 192.168.2.x    */
        default: return 0x08080808;                      /* 8.8.8.8 — no route */
    }
}

/*
 * Sleep for a random interval between 50ms and 300ms.
 * This simulates variable packet inter-arrival times,
 * which is more realistic than a fixed rate.
 */
static void simulate_arrival_delay(void)
{
    int delay_ms = 50 + (rand() % 250);
    struct timespec ts;
    ts.tv_sec  = delay_ms / 1000;
    ts.tv_nsec = (delay_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int g_packet_id_counter = 0;
static pthread_mutex_t g_id_lock = PTHREAD_MUTEX_INITIALIZER;

static int next_packet_id(void)
{
    int id;
    pthread_mutex_lock(&g_id_lock);
    id = ++g_packet_id_counter;
    pthread_mutex_unlock(&g_id_lock);
    return id;
}

void *input_port_thread(void *arg)
{
    InputPortArgs *ctx = (InputPortArgs *)arg;
    char ip_buf[20];

    printf("[Input Port %d] Thread started — will generate %d packets\n",
           ctx->port_id, ctx->num_packets);

    for (int i = 0; i < ctx->num_packets && *(ctx->running); i++) {
        simulate_arrival_delay();

        Packet pkt;
        memset(&pkt, 0, sizeof(Packet));

        pkt.id        = next_packet_id();
        pkt.src_port  = ctx->port_id;
        pkt.src_addr  = 0xC0A80001 + ctx->port_id;  /* 192.168.0.(1+port) */
        pkt.dest_addr = random_dest_addr();
        pkt.priority  = (PacketPriority)(rand() % 3);
        clock_gettime(CLOCK_MONOTONIC, &pkt.created_at);
        snprintf(pkt.payload, MAX_PAYLOAD_SIZE, "DATA-FROM-PORT%d-SEQ%d",
                 ctx->port_id, i);
        pkt.payload_len = strlen(pkt.payload);

        /* Look up the routing table to figure out where this goes */
        int out_port = routing_table_lookup(ctx->rt, pkt.dest_addr);

        if (out_port < 0) {
            /* No matching route — drop the packet */
            snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u",
                     (pkt.dest_addr >> 24) & 0xFF,
                     (pkt.dest_addr >> 16) & 0xFF,
                     (pkt.dest_addr >>  8) & 0xFF,
                      pkt.dest_addr        & 0xFF);
            printf("[Input Port %d] PKT #%03d -> dest %s : NO ROUTE (dropped)\n",
                   ctx->port_id, pkt.id, ip_buf);

            pthread_mutex_lock(&ctx->stats->lock);
            ctx->stats->total_received++;
            ctx->stats->total_dropped++;
            ctx->stats->per_port_sent[ctx->port_id]++;
            pthread_mutex_unlock(&ctx->stats->lock);
            continue;
        }

        pkt.dest_port = out_port;

        snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u",
                 (pkt.dest_addr >> 24) & 0xFF,
                 (pkt.dest_addr >> 16) & 0xFF,
                 (pkt.dest_addr >>  8) & 0xFF,
                  pkt.dest_addr        & 0xFF);
        printf("[Input Port %d] PKT #%03d -> dest %s -> routed to Port %d\n",
               ctx->port_id, pkt.id, ip_buf, out_port);

        /* Enqueue the packet for the output side to pick up */
        if (packet_queue_push(ctx->queue, &pkt, 500) == 0) {
            pthread_mutex_lock(&ctx->stats->lock);
            ctx->stats->total_received++;
            ctx->stats->per_port_sent[ctx->port_id]++;
            pthread_mutex_unlock(&ctx->stats->lock);
        } else {
            printf("[Input Port %d] PKT #%03d : queue full, dropped\n",
                   ctx->port_id, pkt.id);
            pthread_mutex_lock(&ctx->stats->lock);
            ctx->stats->total_received++;
            ctx->stats->total_dropped++;
            pthread_mutex_unlock(&ctx->stats->lock);
        }
    }

    printf("[Input Port %d] Thread finished\n", ctx->port_id);
    return NULL;
}

/* ================================================================
 *  OUTPUT PORT THREAD
 *
 *  Consumes packets from the central queue and "delivers" them.
 *  In a real system this would transmit out a physical interface;
 *  here we just log the routing decision and update stats.
 * ================================================================ */

void *output_port_thread(void *arg)
{
    OutputPortArgs *ctx = (OutputPortArgs *)arg;
    char src_buf[20], dst_buf[20];

    printf("[Output Port %d] Thread started — waiting for packets\n",
           ctx->port_id);

    while (*(ctx->running)) {
        Packet pkt;
        int rc = packet_queue_pop(ctx->queue, &pkt, 200);

        if (rc != 0) {
            /* Timed out, no packet available right now — just loop back */
            continue;
        }

        /* Only process packets destined for our port */
        if (pkt.dest_port != ctx->port_id)  {
            /*
             * Put it back if it's not for us.
             * (A more sophisticated design would use per-port queues,
             *  but this keeps things simpler for demonstration.)
             */
            packet_queue_push(ctx->queue, &pkt, 100);
            /* small yield to avoid spinning on wrong-port packets */
            struct timespec yield_ts = {0, 5000000}; /* 5ms */
            nanosleep(&yield_ts, NULL);
            continue;
        }

        /* Calculate how long this packet spent inside the router */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double latency_ms = ((now.tv_sec - pkt.created_at.tv_sec) * 1000.0)
                          + ((now.tv_nsec - pkt.created_at.tv_nsec) / 1e6);

        snprintf(src_buf, sizeof(src_buf), "%u.%u.%u.%u",
                 (pkt.src_addr >> 24) & 0xFF,
                 (pkt.src_addr >> 16) & 0xFF,
                 (pkt.src_addr >>  8) & 0xFF,
                  pkt.src_addr        & 0xFF);
        snprintf(dst_buf, sizeof(dst_buf), "%u.%u.%u.%u",
                 (pkt.dest_addr >> 24) & 0xFF,
                 (pkt.dest_addr >> 16) & 0xFF,
                 (pkt.dest_addr >>  8) & 0xFF,
                  pkt.dest_addr        & 0xFF);

        printf("[Output Port %d] DELIVERED PKT #%03d | %s -> %s | "
               "pri=%d | latency=%.1fms | \"%s\"\n",
               ctx->port_id, pkt.id, src_buf, dst_buf,
               pkt.priority, latency_ms, pkt.payload);

        pthread_mutex_lock(&ctx->stats->lock);
        ctx->stats->total_routed++;
        ctx->stats->per_port_received[ctx->port_id]++;
        pthread_mutex_unlock(&ctx->stats->lock);
    }

    printf("[Output Port %d] Thread finished\n", ctx->port_id);
    return NULL;
}
