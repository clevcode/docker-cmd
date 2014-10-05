/*
 * Copyright (C) Joel Eriksson <je@clevcode.org> 2014
 */

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <pwd.h>

#include "shared.h"

#define MAX_ARGS 32768

int main(int argc, char **argv)
{
    char jailname[128];
    char username[128];
    char *argp[MAX_ARGS] = {
        "sudo",
        "docker-cmd",
        jailname,
        username,
        NULL
    };
    struct passwd *pw;
    int n;

    if ((pw = getpwuid(getuid())) == NULL) {
        perror("getpwuid");
        return 1;
    }

    if (set_reasonably_secure_env(pw->pw_name) == -1)
        return 2;

    n = snprintf(jailname, sizeof(jailname), "jail_%s", pw->pw_name);
    if (n < 0 || n >= sizeof(jailname)) {
        fprintf(stderr, "snprintf() failed\n");
        return 3;
    }

    n = snprintf(username, sizeof(username), "%s", pw->pw_name);
    if (n < 0 || n >= sizeof(username)) {
        fprintf(stderr, "snprintf() failed\n");
        return 4;
    }

    if (argc > 1) {
        int i = 4;
        argp[i++] = SHELL;
        while (--argc > 0 && i < (sizeof(argp)/sizeof(argp[0]))-1)
            argp[i++] = *++argv;
        argp[i] = NULL;
    }

    close_files(); /* just in case, prevent fd leaks */
    execvp(argp[0], argp);
    perror("execvp");
    return 5;
}
