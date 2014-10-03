# docker-cmd

The docker-cmd project provides an alternative to docker-bash and
dockersh (based on nsenter), that does the right thing and allocates
a new pty from within the target container, so that programs such as
tmux and screen works within the session in question.

Usage: docker-cmd CONTAINER [USER] [CMD]

If no username is provided, it will be set to root, and if no command
is provided, /bin/bash will be used.

The included docker-mkjail script is an example on how to easily set
up a Docker jail for a user. Just run "docker-mkjail luser", to set up
a jail for the user with username "luser". Then set up sudo access by
adding this to your sudoers-file:


```
# Allow user access to Docker jail
luser ALL=(root) NOPASSWD: /path/to/docker-cmd jail_luser luser
```

Change "luser" to the username you want to place in a jail. Then set
the users login shell to /usr/local/bin/docker-jailsh.

By default, the docker-mkjail script sets up an jail container based
on Ubuntu, and disables all SUID/SGID programs within the jail in order
to decrease the chances of performing a privilege escalation attack.
Note that a kernel vulnerability could obviously be abused in order
to break out of the Docker container regardless though.

The reaper-daemon (docker-reaper) is used as the init-process in jail
containers, and simply handles the reaping of zombie processes. When
using docker-cmd to put users in jails, there is usually no need for
daemon processes running from within the jail container, and thus a
real init-process is not necessary. It would be possible to simply run
/bin/bash here, of course, but besides being a waste of CPU cycles and
RAM it would also result in zombie processes not being reaped.

# Future enhancements

Since this was developed primarily with the intention of quickly getting
a working solution for myself and the specific use-case I had in mind,
it is not very configurable. If anyone wants to take the time and add
some getopt()-handling, and possibly a simple configuration file parser,
it would be appreciated. If not, I'll probably add this when I have some
more time to spare.

Things that should be configurable includes the namespaces to clone, what
uid/gid to use, what environment variables to allow, the working directory,
whether to allocate a tty or not (at the moment, it will check if stdin is
connected to a tty to determine whether to allocate one), and so on.

Another feature that I would like to have is for the username parameter
to be interpreted as a username within the container. At the moment, the
UID and GID is determined by looking up the username on the host.

Copyright (C) Joel Eriksson <je@clevcode.org> 2014
