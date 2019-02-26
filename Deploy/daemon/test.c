/*
 * daemonize.c
 * This example daemonizes a process, writes a few log messages,
 * sleeps 20 seconds and terminates afterwards.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

void sighup();
static void skeleton_daemon();
void sig();

int main() {

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

    skeleton_daemon(pid);
    int count = 0;
    while (count <= 100)
    {
        printf("in while\n");
        //TODO: Insert daemon code here.
        syslog (LOG_NOTICE, "First daemon started.");
        sig(pid);
        // sleep (20);
        // break;
        count++;
    }

    syslog (LOG_NOTICE, "First daemon terminated.");
    closelog();

    return EXIT_SUCCESS;
}

void sig(pid_t pid) {
    printf("in trigger\n");
    int ran = rand() % 10;
    sleep(ran);
    kill(pid, SIGHUP);
}

static void skeleton_daemon(pid_t pid)
{

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    // signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    // umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    // chdir("/");

    /* Close all open file descriptors */
    // int x;
    // for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    // {
    //     close (x);
    // }

    /* Open the log file */
    openlog ("firstdaemon", LOG_PID, LOG_DAEMON);
}

// sighup() function definition
void sighup() {
    signal(SIGHUP, sighup); /* reset signal */
    printf("Received a SIGHUP\n");
}
