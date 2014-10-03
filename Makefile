# You might need/want to change the installation path.
DOCKER_PATH = $(shell which docker)
TARGET_PATH = /usr/local/bin

CC      = gcc
CFLAGS  = -O -Wall -ansi -pedantic -std=c99 -DDOCKER_PATH="\"$(DOCKER_PATH)\"" -I.
LDFLAGS = -lutil
TARGETS = docker-cmd docker-jailsh docker-reaper

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%: %.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

all: $(TARGETS)

install: $(TARGETS)
	install -m 755 -o 0 -g 0 $(TARGETS) docker-mkjail $(TARGET_PATH)

clean:
	-$(RM) $(TARGETS) *.o

docker-cmd: shared.o

docker-reaper: docker-reaper.c
	$(CC) $(CFLAGS) -o $@ $^

docker-jailsh: docker-jailsh.c shared.o
	$(CC) $(CFLAGS) -o $@ $^
