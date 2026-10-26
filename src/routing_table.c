/*
 * routing_table.c
 * ---------------
 * Implements the shared routing table with mutex-protected access.
 * Uses longest-prefix matching to resolve destination addresses
 * to output ports, similar to how a real router's FIB works.
 *
 * Author : Prajwal
 * Course : Real-Time Operating Systems (RTOS)
 * Project: Virtual Network Packet Router Simulation (QNX)
 */

#include <stdio.h>
#include <string.h>
#include "routing_table.h"

/*
 * Helper to convert a 32-bit IP address to dotted-decimal string.
 * Writes into the caller-supplied buffer.
 */
static void ip_to_str(uint32_t ip, char *buf, int buf_size)
{
    snprintf(buf, buf_size, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >>  8) & 0xFF,
              ip        & 0xFF);
}

/*
 * Count how many leading 1-bits the mask has.
 * Used for longest-prefix matching — the entry with more
 * leading bits wins when multiple routes match.
 */
static int count_mask_bits(uint32_t mask)
{
    int bits = 0;
    while (mask & 0x80000000) {
        bits++;
        mask <<= 1;
    }
    return bits;
}

void routing_table_init(RoutingTable *rt)
{
    memset(rt, 0, sizeof(RoutingTable));
    pthread_mutex_init(&rt->lock, NULL);

    /*
     * Pre-populate with four subnet routes, one per output port.
     * This gives us a basic forwarding table to work with:
     *
     *   10.0.0.0/8     -> Port 0
     *   172.16.0.0/12  -> Port 1
     *   192.168.1.0/24 -> Port 2
     *   192.168.2.0/24 -> Port 3
     */
    routing_table_add(rt, 0x0A000000, 0xFF000000, 0);   /* 10.0.0.0/8      */
    routing_table_add(rt, 0xAC100000, 0xFFF00000, 1);   /* 172.16.0.0/12   */
    routing_table_add(rt, 0xC0A80100, 0xFFFFFF00, 2);   /* 192.168.1.0/24  */
    routing_table_add(rt, 0xC0A80200, 0xFFFFFF00, 3);   /* 192.168.2.0/24  */
}

int routing_table_lookup(RoutingTable *rt, uint32_t dest_addr)
{
    int best_port = -1;
    int best_prefix_len = -1;

    pthread_mutex_lock(&rt->lock);

    for (int i = 0; i < rt->count; i++) {
        RouteEntry *e = &rt->entries[i];
        if (!e->active)
            continue;

        /* Check if the destination falls within this network */
        if ((dest_addr & e->subnet_mask) == e->dest_network) {
            int prefix_len = count_mask_bits(e->subnet_mask);
            /* Longest prefix match wins */
            if (prefix_len > best_prefix_len) {
                best_prefix_len = prefix_len;
                best_port = e->output_port;
            }
        }
    }

    pthread_mutex_unlock(&rt->lock);
    return best_port;
}

int routing_table_add(RoutingTable *rt, uint32_t network, uint32_t mask, int port)
{
    int result = -1;

    pthread_mutex_lock(&rt->lock);

    if (rt->count < MAX_ROUTES) {
        RouteEntry *e = &rt->entries[rt->count];
        e->dest_network = network;
        e->subnet_mask  = mask;
        e->output_port  = port;
        e->active       = 1;
        rt->count++;
        result = 0;
    }

    pthread_mutex_unlock(&rt->lock);
    return result;
}

void routing_table_print(RoutingTable *rt)
{
    char net_buf[20], mask_buf[20];

    pthread_mutex_lock(&rt->lock);

    printf("\n");
    printf("+------+------------------+------------------+--------+\n");
    printf("| Idx  | Network          | Mask             | Port   |\n");
    printf("+------+------------------+------------------+--------+\n");

    for (int i = 0; i < rt->count; i++) {
        if (!rt->entries[i].active)
            continue;
        ip_to_str(rt->entries[i].dest_network, net_buf, sizeof(net_buf));
        ip_to_str(rt->entries[i].subnet_mask, mask_buf, sizeof(mask_buf));
        printf("| %-4d | %-16s | %-16s | %-6d |\n",
               i, net_buf, mask_buf, rt->entries[i].output_port);
    }

    printf("+------+------------------+------------------+--------+\n\n");

    pthread_mutex_unlock(&rt->lock);
}

void routing_table_destroy(RoutingTable *rt)
{
    pthread_mutex_destroy(&rt->lock);
}
