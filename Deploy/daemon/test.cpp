#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <cstdlib>
using namespace std;

#include "info.h"

void trigger() {
    int random = rand() % 6;
    cout << endl << "Will sleep for " << random << endl << endl;
    sleep(random);
}

int main() {
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

    printf("test start\n");
    while(1)
    {
        FD_ZERO(&read_fd);
        FD_SET(rfd, &read_fd);
        // FD_SET(fileno(stdin), &read_fd);

        net_timer.tv_sec = 5;
        net_timer.tv_usec = 0;
        memset(str, 0, sizeof(str));

        trigger();
        // fgets(str, sizeof(str), stdin);
        Student ns = Student(rand()%10, "Student" + to_string(rand()%10), false);
        string enns = encode(ns);
        strcpy(str, enns.c_str());
        len = write(wfd, str, strlen(str));

        i = select(rfd + 1, &read_fd, NULL, NULL, &net_timer);
        if(i <= 0)
            continue;
        if(FD_ISSET(rfd, &read_fd)){
            read(rfd, str, sizeof(str));
            Student ns = decode(str);
            printf("side:\n");
            prints(ns);
        }
        // if(FD_ISSET(fileno(stdin), &read_fd)) {

        // }
        // close(rfd);
        // close(wfd);
    }
    return 0;
}
