/*
 * routing_table.h
 * ---------------
 * Shared routing table protected by a mutex.
 * Maps destination address ranges to output ports.
 *
 * Author : Prajwal
 * Course : Real-Time Operating Systems (RTOS)
 * Project: Virtual Network Packet Router Simulation (QNX)
 */

#ifndef ROUTING_TABLE_H
#define ROUTING_TABLE_H

#include <pthread.h>
#include <stdint.h>

#define MAX_ROUTES  16

/* A single entry in the routing table */
typedef struct {
    uint32_t dest_network;   /* destination network address        */
    uint32_t subnet_mask;    /* subnet mask for prefix matching    */
    int      output_port;    /* port to forward matching packets   */
    int      active;         /* 1 if this entry is valid, 0 if not */
} RouteEntry;

/* The routing table itself, with mutex protection */
typedef struct {
    RouteEntry      entries[MAX_ROUTES];
    int             count;
    pthread_mutex_t lock;    /* mutex for safe concurrent access   */
} RoutingTable;

/*
 * Initialize the routing table with default routes.
 * Sets up the mutex and populates a few static entries.
 */
void routing_table_init(RoutingTable *rt);

/*
 * Look up which output port a packet should go to,
 * given its destination address.
 * Returns the output port number, or -1 if no match found.
 */
int routing_table_lookup(RoutingTable *rt, uint32_t dest_addr);

/*
 * Add a new route entry to the table.
 * Returns 0 on success, -1 if the table is full.
 */
int routing_table_add(RoutingTable *rt, uint32_t network, uint32_t mask, int port);

/*
 * Print all entries in the routing table to stdout.
 * Acquires the lock internally.
 */
void routing_table_print(RoutingTable *rt);

/*
 * Destroy the routing table mutex when done.
 */
void routing_table_destroy(RoutingTable *rt);

#endif /* ROUTING_TABLE_H */
