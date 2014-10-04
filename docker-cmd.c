/*
 * Copyright (C) Joel Eriksson <je@clevcode.org> 2014
 */

#define _GNU_SOURCE
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <pty.h>

#include "shared.h"

int clone_namespace(pid_t pid, int nstype)
{
    char *type = NULL;
    char fname[32];
    int fd;

    switch (nstype) {
        case CLONE_NEWIPC:  type = "ipc";  break;
        case CLONE_NEWUTS:  type = "uts";  break;
        case CLONE_NEWNET:  type = "net";  break;
        case CLONE_NEWPID:  type = "pid";  break;
        case CLONE_NEWNS:   type = "mnt";  break;
        case CLONE_NEWUSER: type = "user"; break;
    }

    if (type == NULL) {
        fprintf(stderr, "clone_namespace: Invalid nstype (%d)\n", nstype);
        return -1;
    }

    snprintf(fname, sizeof(fname), "/proc/%u/ns/%s", pid, type);

    if ((fd = open(fname, O_RDONLY)) == -1) {
        perror("clone_namespace: open");
        return -1;
    }

    if (setns(fd, nstype) == -1) {
        perror("clone_namespace: setns");
        return -1;
    }

    close(fd);

    return 0;
}

ssize_t writen(int fd, const char *buf, size_t len)
{
    const char *data = buf;
    size_t left = len;
    ssize_t n;

    while (left > 0) {
        if ((n = write(fd, data, left)) == -1) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            perror("writen: write");
            return -1;
        }
        data += n;
        left -= n;
    }

    return (ssize_t) len;
}

int proxy_fds(int fd1, int fd2)
{
    fd_set rfd, wfd, xfd;
    char buf[65536];
    ssize_t n;
    int max;

    max = fd1 < fd2 ? fd2 : fd1;

    for (;;) {
        FD_ZERO(&rfd); 
        FD_ZERO(&wfd); 
        FD_ZERO(&xfd); 

        FD_SET(fd1, &rfd);
        FD_SET(fd2, &rfd);

        if (select(max+1, &rfd, &wfd, &xfd, NULL) == -1) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("proxy_fds: select");
            return -1;
        }

        if (FD_ISSET(fd1, &rfd)) {
            n = read(fd1, buf, sizeof(buf));
            if (n == 0)
                return 0;
            if (n == -1) {
                if (errno != EAGAIN && errno != EINTR) {
                    if (errno == EIO)
                        return 0;
                    perror("proxy_fds: read");
                    return -1;
                }
            } else {
                if (writen(fd2, buf, n) == -1) {
                    perror("proxy_fds: write");
                    return -1;
                }
            }
        }

        if (FD_ISSET(fd2, &rfd)) {
            n = read(fd2, buf, sizeof(buf));
            if (n == 0)
                return 0;
            if (n == -1) {
                if (errno != EAGAIN && errno != EINTR) {
                    if (errno == EIO)
                        return 0;
                    perror("proxy_fds: read");
                    return -1;
                }
            } else {
                if (writen(fd1, buf, n) == -1) {
                    perror("proxy_fds: write");
                    return -1;
                }
            }
        }
    }
}

static struct termios org_termios;
static int tty_saved;

int tty_save()
{
    if (tcgetattr(STDIN_FILENO, &org_termios) == -1) {
        perror("tty_save: tcgetattr");
        return -1;
    }

    tty_saved = 1;

    return 0;
}

int tty_restore()
{
    if (tty_saved) {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &org_termios) == -1) {
            perror("tty_restore: tcsetattr");
            return -1;
        }
    }

    return 0;
}

int tty_raw()
{
    struct termios org_termios;
    struct termios raw;

    if (! tty_saved)
        if (tty_save() == -1)
            return -1;

    raw = org_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tty_raw: tcsetattr");
        return -1;
    }

    return 0;
}

int get_window_size(int tty_fd, struct winsize *w)
{
    if (ioctl(tty_fd, TIOCGWINSZ, w) == -1) {
        perror("get_window_size: ioctl");
        return -1;
    }

    return 0;
}

int set_window_size(int tty_fd, struct winsize *w)
{
    if (ioctl(tty_fd, TIOCSWINSZ, w) == -1) {
        perror("set_window_size: ioctl");
        return -1;
    }

    return 0;
}

int clone_window_size(int src_tty, int dst_tty)
{
    struct winsize w;

    if (get_window_size(src_tty, &w) == -1)
        return -1;

    if (set_window_size(dst_tty, &w) == -1)
        return -1;

    return 0;
}

int pty_fd;

void window_size_changed(int sig)
{
    clone_window_size(STDIN_FILENO, pty_fd);
}

/*
 * We don't want to use popen() here, since that will also invoke a shell.
 * No reason to increase the attack surface more than necessary.
 */
FILE *execve_pipe(const char *fname, const char **argv, const char **envp)
{
    int fds[2];
    pid_t pid;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
        perror("execve_pipe: socketpair");
        return NULL;
    }

    if ((pid = vfork()) == -1) {
        perror("execve_pipe: vfork");
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }

    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], 0);
        dup2(fds[1], 1);
        close(fds[1]);
        close_files(); /* just in case, prevent fd leaks */
        execve(fname, (char * const*) argv, (char * const*) envp);
        _exit(1);
    }

    close(fds[1]);
    return fdopen(fds[0], "r+");
}

pid_t get_docker_container_pid(const char *name)
{
    const char *fname = DOCKER_PATH;
    const char *argp[] = {
        "docker",
        "inspect",
        "--format",
        "{{.State.Pid}}",
        name,
        NULL
    };
    const char *envp[] = {
        "PATH=/bin:/usr/bin:/sbin:/usr/sbin",
        NULL
    };
    pid_t pid;
    FILE *fp;

    if ((fp = execve_pipe(fname, argp, envp)) == NULL)
        return (pid_t) -1;

    errno = 0;
    if (fscanf(fp, "%d", &pid) != 1) {
        if (errno != 0)
            perror("get_docker_container_pid: fscanf");
        else
            fprintf(stderr, "get_docker_container_pid: Could not find a PID for container '%s'\n", name);
        return (pid_t) -1;
    }

    fclose(fp);

    return pid;
}

int main(int argc, char **argv)
{
    char *def_argp[] = { "/bin/bash", NULL };
    int set_supplemental_groups = 0;
    const char *username = "root";
    char **argp = def_argp;
    struct passwd *pw;
    int status;
    int is_tty;
    pid_t pid;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s CONTAINER [USERNAME] [CMD...]\n", argv[0]);
        return 1;
    }

    if ((pid = get_docker_container_pid(argv[1])) == (pid_t) -1)
        return 2;

    if (argc > 2)
        username = argv[2];

    if (argc > 3)
        argp = &argv[3];

    is_tty = isatty(0);

    if (set_reasonably_secure_env(username) == -1)
        return 3;

    if ((pw = getpwnam(username)) == NULL) {
        fprintf(stderr, "No such user: %s\n", username);
        return 4;
    }

    /*
     * When docker adds support for CLONE_NEWUSER, that should be added as well.
     */
    if (clone_namespace(pid, CLONE_NEWIPC) == -1
    ||  clone_namespace(pid, CLONE_NEWUTS) == -1
    ||  clone_namespace(pid, CLONE_NEWNET) == -1
    ||  clone_namespace(pid, CLONE_NEWPID) == -1
    ||  clone_namespace(pid, CLONE_NEWNS)  == -1)
        return 5;

    if (setregid(pw->pw_gid, pw->pw_gid) == -1) {
        perror("setregid");
        return 6;
    }

    if (set_supplemental_groups) {
        if (initgroups(argv[2], pw->pw_gid) == -1) {
            perror("initgroups");
            return 7;
        }
    } else {
        if (setgroups(1, &pw->pw_gid) == -1) {
            perror("setgroups");
            return 7;
        }
    }

    if (setreuid(pw->pw_uid, pw->pw_uid) == -1) {
        perror("setreuid");
        return 8;
    }

    if (chdir(pw->pw_dir) == -1) {
        perror("chdir");
        return 9;
    }

    if (is_tty)
        pid = forkpty(&pty_fd, NULL, NULL, NULL);
    else
        pid = fork();

    if (pid == -1) {
        perror("fork");
        return 10;
    }

    if (pid == 0) {
        close_files(); /* just in case, prevent fd leaks */
        execvp(argp[0], argp);
        perror("execvp");
        return -1;
    }

    if (is_tty) {
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = window_size_changed;
        if (sigaction(SIGWINCH, &sa, NULL) == -1) {
            perror("sigaction");
            return 11;
        }
        clone_window_size(STDIN_FILENO, pty_fd);
        tty_raw();
        proxy_fds(STDIN_FILENO, pty_fd);
        tty_restore();
    }

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 12;
    }

    return WEXITSTATUS(status);
}
