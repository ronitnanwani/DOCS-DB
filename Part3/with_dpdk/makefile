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

TARGET="dpdk_server"
all:
	g++ -std=c++20 -c lsm.cpp -o lsm.o -pthread
	gcc ${CFLAGS} -c dpdk_server.c ${LIBS}
	g++ -std=c++20 dpdk_server.o lsm.o -o ${TARGET} ${LIBS}
