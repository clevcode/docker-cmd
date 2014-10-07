/*
 * Copyright (C) Joel Eriksson <je@clevcode.org> 2014
 */

#define _POSIX_SOURCE
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

void on_sigchld(int sig)
{
    int rc;

    do {
        if ((rc = waitpid(-1, 0, WNOHANG)) == -1 && errno != ECHILD)
            perror("waitpid");
    } while (rc > 0);
}

int main(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_sigchld;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    for (;;)
        pause();

    /* will not be reached */
    return 1;
}
