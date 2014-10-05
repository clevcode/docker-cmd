/*
 * Copyright (C) Joel Eriksson <je@clevcode.org> 2014
 */

#define _GNU_SOURCE
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>

int set_reasonably_secure_env(const char *username)
{
    static char home_buf[PATH_MAX+6], term_buf[64], path_buf[128], *term_str;
    struct passwd *pw;
    int n;

    errno = 0;
    if ((pw = getpwnam(username)) == NULL) {
        if (errno != 0)
            perror("set_reasonably_secure_env: getpwnam");
        else
            fprintf(stderr, "set_reasonably_secure_env: getpwnam: User not found\n");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    if (clearenv() == -1) {
        perror("set_reasonably_secure_env: clearenv");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    n = snprintf(home_buf, sizeof(home_buf), "HOME=%s", pw->pw_dir);
    if (n < 0 || n >= sizeof(home_buf)) {
        fprintf(stderr, "set_reasonably_secure_env: snprintf() failed\n");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    if (pw->pw_uid == 0)
        n = snprintf(path_buf, sizeof(path_buf), "PATH=/bin:/usr/bin:/sbin:/usr/sbin");
    else
        n = snprintf(path_buf, sizeof(path_buf), "PATH=/bin:/usr/bin");

    if (n < 0 || n >= sizeof(path_buf)) {
        fprintf(stderr, "set_reasonably_secure_env: snprintf() failed\n");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    if ((term_str = getenv("TERM")) == NULL)
        term_str = "xterm";

    if (! strncmp(term_str, "() {", 4)) {
        fprintf(stderr, "set_reasonably_secure_env: Possible CVE-2014-6271 exploit attempt\n");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    snprintf(term_buf, sizeof(term_buf), "TERM=%s", term_str);

    if (n < 0 || n >= sizeof(term_buf)) {
        fprintf(stderr, "set_reasonably_secure_env: snprintf() failed\n");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    putenv(home_buf);
    putenv(path_buf);
    putenv(term_buf);

    /*
     * Might want to add a few more whitelisted environment variables,
     * such as LANG / LC_*. But I'd rather be safe than sorry, so until
     * configuration options / configuration files have been added to
     * specify this, we keep it strict.
     */

    return 0;
}

int close_files()
{
    struct rlimit rlim;
    rlim_t fd;

    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
        perror("close_files: getrlimit");
        _exit(1); /* rather not give the calling function a chance to mess this up */
    }

    for (fd = 3; fd < rlim.rlim_cur; fd++)
        if (close(fd) == -1 && errno != EBADF) {
		perror("close_files: close");
		_exit(1); /* rather not give the calling function a chance to mess this up */
	}

    return 0;
}
