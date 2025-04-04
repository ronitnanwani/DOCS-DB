#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/resource.h>  // For getrusage()
#include <time.h>  // For time()

#include "ff_config.h"
#include "ff_api.h"

#define MAX_EVENTS 1024
#define MONITOR_INTERVAL 10 // Time interval to print server usage (in seconds)

/* kevent set */
struct kevent kevSet;
/* events */
struct kevent events[MAX_EVENTS];
/* kq */
int kq;
int sockfd;

// Function to calculate time difference in seconds
double get_time_diff(struct timeval start, struct timeval end) {
    return ((end.tv_sec - start.tv_sec) * 1000.0) + ((end.tv_usec - start.tv_usec) / 1000.0);
}

void print_server_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    printf("CPU Usage: user time = %ld.%06ld, system time = %ld.%06ld\n",
           usage.ru_utime.tv_sec, usage.ru_utime.tv_usec,
           usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
    printf("Memory Usage: %ld KB\n", usage.ru_maxrss);
}

int loop(void *arg)
{
    int nevents = ff_kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
    int i;

    static time_t last_monitor_time = 0;

    if (nevents < 0) {
        printf("ff_kevent failed:%d, %s\n", errno, strerror(errno));
        return -1;
    }

    for (i = 0; i < nevents; ++i) {
        struct kevent event = events[i];
        int clientfd = (int)event.ident;

        /* Handle disconnect */
        if (event.flags & EV_EOF) {
            ff_close(clientfd);
        } else if (clientfd == sockfd) {
            int available = (int)event.data;
            do {
                int nclientfd = ff_accept(clientfd, NULL, NULL);
                if (nclientfd < 0) {
                    printf("ff_accept failed:%d, %s\n", errno, strerror(errno));
                    break;
                }

                EV_SET(&kevSet, nclientfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                if (ff_kevent(kq, &kevSet, 1, NULL, 0, NULL) < 0) {
                    printf("ff_kevent error:%d, %s\n", errno, strerror(errno));
                    return -1;
                }

                available--;
            } while (available);
        } else if (event.filter == EVFILT_READ) {
            char buf[1024]; // Buffer size large enough for 1KB of data
            ssize_t readlen = ff_read(clientfd, buf, sizeof(buf));
            
            if (readlen < 0) {
                printf("ff_read failed:%d, %s\n", errno, strerror(errno));
                ff_close(clientfd);
                continue;
            }

            // Send back the length of the data received
            char reply[64]; // Buffer to store the response (length of data)
            int reply_len = snprintf(reply, sizeof(reply), "Received %ld bytes\n", readlen);
            
            ssize_t writelen = ff_write(clientfd, reply, reply_len);
            if (writelen < 0) {
                printf("ff_write failed:%d, %s\n", errno, strerror(errno));
                ff_close(clientfd);
            }
        } else {
            printf("unknown event: %8.8X\n", event.flags);
        }

        // Periodically print server CPU and memory usage (every MONITOR_INTERVAL seconds)
        time_t current_time = time(NULL);
        if (current_time - last_monitor_time >= MONITOR_INTERVAL) {
            last_monitor_time = current_time;
            print_server_usage();
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    ff_init(argc, argv);

    kq = ff_kqueue();
    if (kq < 0) {
        printf("ff_kqueue failed, errno:%d, %s\n", errno, strerror(errno));
        exit(1);
    }

    sockfd = ff_socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("ff_socket failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    // Set non-blocking
    int on = 1;
    ff_ioctl(sockfd, FIONBIO, &on);

    struct sockaddr_in my_addr;
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(80);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = ff_bind(sockfd, (struct linux_sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        printf("ff_bind failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    ret = ff_listen(sockfd, MAX_EVENTS);
    if (ret < 0) {
        printf("ff_listen failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    EV_SET(&kevSet, sockfd, EVFILT_READ, EV_ADD, 0, MAX_EVENTS, NULL);
    ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);
    ff_run(loop, NULL);

    return 0;
}