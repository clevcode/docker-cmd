/*
 * Copyright (C) Joel Eriksson <je@clevcode.org> 2014
 */

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
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

    if ((pw = getpwuid(getuid())) == NULL) {
        perror("getpwuid");
        return 1;
    }

    if (set_reasonably_secure_env(pw->pw_name) == -1)
        return 2;

    snprintf(jailname, sizeof(jailname), "jail_%s", pw->pw_name);
    snprintf(username, sizeof(username), "%s", pw->pw_name);

    if (argc > 1) {
        int i = 4;
        argp[i++] = SHELL;
        while (--argc > 0 && i < (sizeof(argp)/sizeof(argp[0]))-1)
            argp[i++] = *++argv;
        argp[i] = NULL;
    }

    close_files(); /* just in case, prevent fd leaks */
    execvp("sudo", argp);
    perror("execvp");
    return 1;
}
