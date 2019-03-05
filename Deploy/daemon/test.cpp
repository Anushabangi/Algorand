/*
 * daemonize.c
 * This example daemonizes a process, writes a few log messages,
 * sleeps 20 seconds and terminates afterwards.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <syslog.h>
#include <iostream>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "info.h"
using namespace std;

int TIME_LIMIT = 20;

static void skeleton_daemon() {
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    // /* Catch, ignore and handle signals */
    // //TODO: Implement a working signal handler */
    // signal(SIGCHLD, SIG_IGN);
    // signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // // /* Set new file permissions */
    // umask(0);
    // //
    // // /* Change the working directory to the root directory */
    // // /* or another appropriated directory */
    // chdir("/");
    // //
    // // /* Close all open file descriptors */
    // int x;
    // for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    // {
    //     close (x);
    // }
}

int main() {
    // skeleton_daemon();
    int i, rfd, wfd, len = 0, fd_in;
    char str[32];
    int flag, stdinflag;
    fd_set write_fd, read_fd;
    struct timeval net_timer;

    mkfifo("fifo1", S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
    mkfifo("fifo2", S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
    wfd = open("fifo1", O_WRONLY);
    rfd = open("fifo2", O_RDONLY);
    if(rfd <= 0 || wfd <= 0) return 0;

    // clock_t endTime = clock() + (TIME_LIMIT * CLOCKS_PER_SEC);
    // cout << "Daemon is starting." << endl;
    printf("test start\n");
    while(1)
    // while (clock() < endTime)
    {
        //TODO: Insert daemon code here.
        // if((endTime - clock()) % (5 * CLOCKS_PER_SEC) == 0)
        //     cout << "Count down:" << (endTime - clock()) / CLOCKS_PER_SEC << endl;

        FD_ZERO(&read_fd);
        FD_SET(rfd, &read_fd);
        FD_SET(fileno(stdin), &read_fd);

        net_timer.tv_sec = 5;
        net_timer.tv_usec = 0;
        memset(str, 0, sizeof(str));
        i = select(rfd + 1, &read_fd, NULL, NULL, &net_timer);
        if(i <= 0)
            continue;
        if(FD_ISSET(rfd, &read_fd)){
            read(rfd, str, sizeof(str));
            printf("side:%s\n", str);
        }
        if(FD_ISSET(fileno(stdin), &read_fd)) {
            fgets(str, sizeof(str), stdin);
            len = write(wfd, str, strlen(str));
        }
        // close(rfd);
        // close(wfd);
    }
    // cout << "Daemon terminated." << endl;

    return EXIT_SUCCESS;
}
