#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pwd.h>

#include "shared.h"

int main(void)
{
    char jailname[128];
    char username[128];
    char *argp[] = {
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

    close_files(); /* just in case, prevent fd leaks */
    execvp("sudo", argp);
    perror("execvp");
    return 1;
}
