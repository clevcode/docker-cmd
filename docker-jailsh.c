#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pwd.h>

int main(void)
{
    char jailname[128];
    char username[128];
    char *argp[] = {
        "sudo",
        "docker-cmd",
        jailname,
        username
    };
    struct passwd *pw;
    
    if ((pw = getpwuid(getuid())) == NULL) {
        perror("getpwuid");
        return 1;
    }

    snprintf(jailname, sizeof(jailname), "jail_%s", pw->pw_name);
    snprintf(username, sizeof(username), "%s", pw->pw_name);

    execvp("sudo", argp);
    perror("execvp");
    return 1;
}
