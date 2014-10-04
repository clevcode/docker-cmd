docker-cmd
==========

The docker-cmd project provides an alternative to docker-bash and
dockersh (based on nsenter), that does the right thing and allocates
a new pty from within the target container, so that programs such as
tmux and screen works within the session in question.

Usage: docker-cmd CONTAINER USER CMD ARGS...

If no username is provided, it will be set to root. If no command
is provided, /bin/bash is used.

The docker-mkjail and docker-jailsh tools are provided in order to
simplify the process of setting up a jail for SSH users (for example).
For more information about this, look at the "Setting up a Docker jail"
section.

Installation
============

To download, build and install the docker-cmd tools, run:
```
git clone https://github.com/clevcode/docker-cmd.git
cd docker-cmd
make clean all
sudo make install
```

Basic usage
===========

To use docker-cmd as a replacement for docker-bash or docker-enter,
simply use it like this:
```
# To enter the container with name CONTAINER as root and run /bin/bash
docker-cmd CONTAINER

# To run echo Hello World from within CONTAINER as user nobody
docker-cmd CONTAINER nobody run echo Hello World
```
The docker-reaper command can be used as the init-process in containers
that do not require any daemons (and note that running an SSH daemon from
within the container is quite unnecessary, when you can simply use
docker-cmd instead). Example:
```
je@seth:~$ docker run -h test --name test -v $(which docker-reaper):/sbin/reaper:ro \
  -d ubuntu:trusty /sbin/reaper
20096f6b9871052f54687632d6cf12d9671bbf6a2d9792b2f7eac787b8d013f3
je@seth:~$ tty
/dev/pts/22
je@seth:~$ sudo docker-cmd test
root@test:~# ps auxw
USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
root         1  0.0  0.0   4192   356 ?        Ss   13:05   0:00 /sbin/reaper
root         8  0.0  0.0  18168  1876 pts/0    Ss   13:05   0:00 /bin/bash
root        18  0.0  0.0  15568  1124 pts/0    R+   13:05   0:00 ps auxw
root@test:~# tty
/dev/pts/0
```
As you can see, unlike most other solutions, a new pty will be allocated
within the target container so that commands such as tmux and screen work
fine. With docker-bash and docker-enter, you would see "not a tty" as the
output from the tty-command, since the pty connected to the session is
located outside the Docker container namespace.

Setting up a Docker jail
========================

To create a jail for the user with username "luser", configure sudo to
allow entering the jail, and change the login shell for the user so that
the jail is automatically entered when logging in, run:
```
user=luser
sudo docker-mkjail $user
cat << EOF | sudo tee /etc/sudoers.d/jail_$user
$user ALL=(root) NOPASSWD: $(which docker-cmd) jail_$user $user
$user ALL=(root) NOPASSWD: $(which docker-cmd) jail_$user $user *
EOF
sudo chsh -s $(which docker-jailsh) $user
```
The first time you create a jail, it will create a new base image. This will
take some time. The next time you create a jail for a user, this image will
be reused. If you want to remove a jail base image, in order to create a new
one, you should run (assuming the base image name is "jail"):
```
sudo docker rm jail
sudo docker rmi jail
```
If you want to disable the jail for a user named "luser", and restore the login shell to /bin/bash, run:
```
user=luser
sudo docker stop jail_$user
sudo docker rm jail_$user
sudo rm /etc/sudoers.d/jail_$user
sudo chsh -s /bin/bash $user
```

These command sequences could obviously be placed in scripts for convenience.

Notes
=====

You might want to customize the docker-mkjail script, if you want to
use another Docker base image or change the commands that are executed
to set it up. By default, it will set up a jail container based on
Ubuntu (Trusty), and disable all SUID/SGID programs within the jail in
order to decrease the chances of performing a privilege escalation
attack. Another thing you might want to do is add more shared folders,
by adding -v /path/on/host:/path/in/guest:rw to the docker run line.
They will by default only have access to their own home directory.

Note that a kernel vulnerability could obviously be abused in order to
break out of the Docker container regardless. The only way to be fully
secure is to not keep any sensitive data/services on the same host as
you are letting people log in to, even when using this.

The reaper-daemon (docker-reaper) is used as the init-process in jail
containers, and simply handles the reaping of zombie processes. When
using docker-cmd to put users in jails, there is usually no need for
daemon processes running from within the jail container, and thus a
real init-process is not necessary. It would be possible to simply run
/bin/bash here, of course, but besides being a waste of CPU cycles and
RAM it would also result in zombie processes not being reaped.

Future enhancements
===================

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

Security
========

While I have tried hard to minimize the risk of this being abused, I make
no guarantees about the security of this solution. Since my primary area of
expertise is within the offensive side of IT-security, rather than the
defensive, I know how futile it might be to make any such guarantees.
There will always be factors beyond my control, such as kernel vulnerabilities.
That being said, I believe I have done my part in minimizing the possible
attack vectors. :)

Copyright (C) Joel Eriksson <je@clevcode.org> 2014
