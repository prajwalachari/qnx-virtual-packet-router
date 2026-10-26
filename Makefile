#
# Makefile for Virtual Network Packet Router Simulation
#
# Supports two build modes:
#   make       -> builds using GCC (for testing on Linux/POSIX systems)
#   make qnx   -> builds using QNX qcc compiler for target deployment
#
# Author : Prajwal
# Course : Real-Time Operating Systems (RTOS)
#

# ---- Compiler selection ----
CC       = gcc
QCC      = qcc
CFLAGS   = -Wall -Wextra -pthread -I./include
QFLAGS   = -Vgcc_ntox86_64 -Wall -Wextra -I./include

# ---- Source and output ----
SRC_DIR  = src
INC_DIR  = include
OBJ_DIR  = obj
BIN      = packet_router

SOURCES  = $(SRC_DIR)/main.c \
           $(SRC_DIR)/router.c \
           $(SRC_DIR)/routing_table.c

OBJECTS  = $(OBJ_DIR)/main.o \
           $(OBJ_DIR)/router.o \
           $(OBJ_DIR)/routing_table.o

# ---- Default target (GCC/Linux) ----
all: dirs $(BIN)
	@echo "Build complete: ./$(BIN)"

$(BIN): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- QNX target ----
qnx: dirs
	$(QCC) $(QFLAGS) -o $(BIN)_qnx $(SOURCES) -lpthread
	@echo "QNX build complete: ./$(BIN)_qnx"

# ---- Utility targets ----
dirs:
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN) $(BIN)_qnx

run: all
	./$(BIN)

.PHONY: all qnx dirs clean run
