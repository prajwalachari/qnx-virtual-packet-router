/*
 * test_routing.c - basic tests for routing table lookups
 *
 * Build: gcc -Wall -I../include -o test_routing test_routing.c ../src/routing_table.c -lpthread
 * Run  : ./test_routing
 */

#include <stdio.h>
#include <stdlib.h>
#include "routing_table.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void check(const char *label, int got, int expected)
{
    if (got == expected) {
        printf("  [PASS] %s : got %d\n", label, got);
        tests_passed++;
    } else {
        printf("  [FAIL] %s : expected %d, got %d\n", label, expected, got);
        tests_failed++;
    }
}

int main(void)
{
    RoutingTable rt;
    routing_table_init(&rt);

    printf("Running routing table tests...\n\n");

    /* Test lookups against the default routes */
    check("10.0.0.1 -> port 0",
          routing_table_lookup(&rt, 0x0A000001), 0);

    check("10.255.255.254 -> port 0",
          routing_table_lookup(&rt, 0x0AFFFFFE), 0);

    check("172.16.0.1 -> port 1",
          routing_table_lookup(&rt, 0xAC100001), 1);

    check("172.31.255.254 -> port 1",
          routing_table_lookup(&rt, 0xAC1FFFFE), 1);

    check("192.168.1.100 -> port 2",
          routing_table_lookup(&rt, 0xC0A80164), 2);

    check("192.168.2.50 -> port 3",
          routing_table_lookup(&rt, 0xC0A80232), 3);

    /* This address shouldn't match anything */
    check("8.8.8.8 -> no route (-1)",
          routing_table_lookup(&rt, 0x08080808), -1);

    check("1.1.1.1 -> no route (-1)",
          routing_table_lookup(&rt, 0x01010101), -1);

    /* Test adding a new route and looking it up */
    routing_table_add(&rt, 0x08080800, 0xFFFFFF00, 2);  /* 8.8.8.0/24 -> port 2 */

    check("8.8.8.8 after adding 8.8.8.0/24 -> port 2",
          routing_table_lookup(&rt, 0x08080808), 2);

    /* The old no-match address should still fail */
    check("1.1.1.1 still no route (-1)",
          routing_table_lookup(&rt, 0x01010101), -1);

    printf("\n---\nResults: %d passed, %d failed\n",
           tests_passed, tests_failed);

    routing_table_destroy(&rt);

    return tests_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
