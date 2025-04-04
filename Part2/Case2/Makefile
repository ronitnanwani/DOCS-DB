CC = gcc
CFLAGS = -Wall -g  # Enable debugging symbols
SERVER = dpdk_server
CLIENT = dpdk_client

ifeq ($(FF_PATH),)
	FF_PATH=${TOPDIR}
endif

ifneq ($(shell pkg-config --exists libdpdk && echo 0),0)
$(error "No installation of DPDK found, maybe you should export environment variable `PKG_CONFIG_PATH`")
endif

PKGCONF ?= pkg-config

CFLAGS += -O0 -g -gdwarf-2 $(shell $(PKGCONF) --cflags libdpdk)

LIBS+= $(shell $(PKGCONF) --static --libs libdpdk)
LIBS+= -L${FF_PATH}/lib -Wl,--whole-archive,-lfstack,--no-whole-archive
LIBS+= -Wl,--no-whole-archive -lrt -lm -ldl -lcrypto -pthread -lnuma

setup: 
	sudo bash setup.sh

server: dpdk_server.c
	@echo "Compiling the DPDK server..."
	$(CC) ${CFLAGS} -o ${SERVER} dpdk_server.c ${LIBS}

client: dpdk_client.c
	@echo "Compiling the DPDK client..."
	$(CC) $(CFLAGS) -o $(CLIENT) dpdk_client.c

run_server: server
	@echo "Running the DPDK server..."
	sudo ./dpdk_server --conf config.ini --proc-type=primary --proc-id=0

run_client: client
	@echo "Running the DPDK client..."
	sudo ./dpdk_client