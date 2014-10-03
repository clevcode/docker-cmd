# You might need/want to configure these paths.
DOCKER_PATH = /usr/bin/docker
TARGET_PATH = /usr/local/bin

CC      = gcc
CFLAGS  = -O -Wall -ansi -pedantic -std=c99 -DDOCKER_PATH="\"$(DOCKER_PATH)\""
LDFLAGS = -lutil
TARGETS = docker-cmd docker-reaper docker-jailsh

%: %.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

all: $(TARGETS)

install:
	install -m 755 -o 0 -g 0 $(TARGETS) docker-mkjail $(TARGET_PATH)

clean:
	-$(RM) $(TARGETS)

docker-reaper: docker-reaper.c
	$(CC) $(CFLAGS) -o $@ $^

docker-jailsh: docker-jailsh.c
	$(CC) $(CFLAGS) -o $@ $^
