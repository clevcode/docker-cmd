/*
 * Copyright (C) Joel Eriksson <je@clevcode.org> 2014
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>

extern char **environ;

int set_reasonably_secure_env(const char *username)
{
    static char home_buf[PATH_MAX+6], term_buf[64], path_buf[128], *term_str;
    struct passwd *pw;

    errno = 0;
    if ((pw = getpwnam(username)) == NULL) {
        if (errno != 0)
            perror("set_reasonably_secure_env: getpwnam");
        else
            fprintf(stderr, "set_reasonably_secure_env: getpwnam: User not found\n");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    clearenv();

    snprintf(home_buf, sizeof(home_buf), "HOME=%s", pw->pw_dir);

    if (pw->pw_uid == 0)
        snprintf(path_buf, sizeof(path_buf), "PATH=/bin:/usr/bin:/sbin:/usr/sbin");
    else
        snprintf(path_buf, sizeof(path_buf), "PATH=/bin:/usr/bin");

    if ((term_str = getenv("TERM")) == NULL)
        term_str = "xterm";

    if (! strncmp(term_str, "() {", 4)) {
        fprintf(stderr, "set_reasonably_secure_env: Possible CVE-2014-6271 exploit attempt\n");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    snprintf(term_buf, sizeof(term_buf), "TERM=%s", term_str);

    putenv(home_buf);
    putenv(path_buf);
    putenv(term_buf);

    /*
     * Might want to add a few more whitelisted environment variables,
     * such as LANG / LC_CTYPE etc. But I'd rather be safe than sorry,
     * so until configuration options / configuration files have been
     * added to specify this, we keep it strict.
     */

    return 0;
}
