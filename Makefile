# CS 118 Project 1 Makefile

CC = gcc
CFLAGS = -O4 -g -Wall -Wextra -Werror -Wno-unused
#CFLAGS = -pthread -g -Wall -Wextra
DIR = proj1-$(USER)

CLIENT_SOURCES = client.c
SERVER_SOURCES = server.c

CLIENT_OBJECTS = $(subst .c,.o,$(CLIENT_SOURCES))
SERVER_OBJECTS = $(subst .c,.o,$(SERVER_SOURCES))

DIST_SOURCES = $(CLIENT_SOURCES) $(SERVER_SOURCES) Makefile README

all: client server

client: $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJECTS)

server: $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJECTS)

# dist: $(DIR).tar.gz

$(DIR).tar.gz: $(DIST_SOURCES)
	rm -rf $(DIR)
	tar -czf $@.tmp --transform='s,^,$(DIR)/,' $(DIST_SOURCES)
	./checkdist $(DIR)
	mv $@.tmp $@

# check: test.sh
# 	./test.sh

clean:
	rm -rf *~ *.o *.tar.gz client server $(DIR) *.tmp

.PHONY: all client server clean