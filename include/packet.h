/*
 * packet.h - network packet data structure
 */

#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <time.h>

#define MAX_PAYLOAD_SIZE  64
#define MAX_PORTS          4

/* Priority levels for QoS-aware routing */
typedef enum {
    PRIORITY_LOW    = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_HIGH   = 2
} PacketPriority;

/* Represents a single network packet flowing through the router */
typedef struct {
    int             id;                         /* unique packet identifier       */
    int             src_port;                   /* input port number (0..3)       */
    int             dest_port;                  /* resolved output port (0..3)    */
    uint32_t        src_addr;                   /* source IP (simulated)          */
    uint32_t        dest_addr;                  /* destination IP (simulated)     */
    PacketPriority  priority;                   /* traffic priority class         */
    char            payload[MAX_PAYLOAD_SIZE];  /* payload data                   */
    int             payload_len;                /* actual bytes used in payload   */
    struct timespec created_at;                 /* timestamp when packet arrived  */
} Packet;

#endif /* PACKET_H */
