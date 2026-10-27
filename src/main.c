/*
 * main.c - entry point for the packet router simulation
 * Sets up routing table, spawns input/output threads, prints stats.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "packet.h"
#include "routing_table.h"
#include "router.h"

/* Global flag to coordinate graceful shutdown */
static volatile int g_running = 1;

/* Signal handler — pressing Ctrl+C stops the simulation cleanly */
static void signal_handler(int sig)
{
    (void)sig;
    printf("\n[Main] Caught interrupt signal, shutting down...\n");
    g_running = 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Seed the random number generator with current time */
    srand((unsigned int)time(NULL));

    /* Register signal handler for clean shutdown */
    signal(SIGINT, signal_handler);

    printf("============================================================\n");
    printf("    Virtual Network Packet Router Simulation (QNX RTOS)     \n");
    printf("    RTOS Concepts: Threads, Mutex, Message Passing, Timers  \n");
    printf("============================================================\n\n");

    /* set up routing table */
    RoutingTable routing_table;
    routing_table_init(&routing_table);

    printf("[Main] Routing table initialized with default routes:\n");
    routing_table_print(&routing_table);

    /* init the shared queue */
    PacketQueue central_queue;
    packet_queue_init(&central_queue);
    printf("[Main] Packet queue initialized (capacity: %d)\n\n", PACKET_QUEUE_SIZE);

    /* stats */
    RouterStats stats;
    router_stats_init(&stats);

    /* spawn input port threads */
    pthread_t input_threads[MAX_PORTS];
    InputPortArgs input_args[MAX_PORTS];

    int packets_per_port = TOTAL_PACKETS / MAX_PORTS;

    printf("[Main] Starting %d input port threads (%d packets each)...\n",
           MAX_PORTS, packets_per_port);

    for (int i = 0; i < MAX_PORTS; i++) {
        input_args[i].port_id     = i;
        input_args[i].num_packets = packets_per_port;
        input_args[i].rt          = &routing_table;
        input_args[i].queue       = &central_queue;
        input_args[i].stats       = &stats;
        input_args[i].running     = (int *)&g_running;

        if (pthread_create(&input_threads[i], NULL,
                           input_port_thread, &input_args[i]) != 0) {
            perror("Failed to create input port thread");
            return EXIT_FAILURE;
        }
    }

    /* spawn output port threads */
    pthread_t output_threads[MAX_PORTS];
    OutputPortArgs output_args[MAX_PORTS];

    printf("[Main] Starting %d output port threads...\n\n", MAX_PORTS);

    for (int i = 0; i < MAX_PORTS; i++) {
        output_args[i].port_id = i;
        output_args[i].queue   = &central_queue;
        output_args[i].stats   = &stats;
        output_args[i].running = (int *)&g_running;

        if (pthread_create(&output_threads[i], NULL,
                           output_port_thread, &output_args[i]) != 0) {
            perror("Failed to create output port thread");
            return EXIT_FAILURE;
        }
    }

    printf("[Main] All threads running. Simulation in progress...\n");
    printf("       (Press Ctrl+C to stop early)\n\n");

    /* wait for input threads to finish */
    for (int i = 0; i < MAX_PORTS; i++) {
        pthread_join(input_threads[i], NULL);
    }

    printf("\n[Main] All input ports done generating packets.\n");
    printf("[Main] Waiting for output ports to drain the queue...\n");

    /* Give output threads a bit more time to process remaining packets */
    sleep(2);

    /* Signal output threads to stop */
    g_running = 0;

    for (int i = 0; i < MAX_PORTS; i++) {
        pthread_join(output_threads[i], NULL);
    }

    /* print final stats */
    router_stats_print(&stats);

    /* cleanup */
    packet_queue_destroy(&central_queue);
    routing_table_destroy(&routing_table);
    router_stats_destroy(&stats);

    printf("[Main] Simulation complete. All resources freed.\n");
    return EXIT_SUCCESS;
}
