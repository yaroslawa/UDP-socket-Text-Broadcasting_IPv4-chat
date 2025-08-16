CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11
SRCS = main.c
OBJS = $(SRCS:.c=.o)
ipv4 = ipv4-chat

all: ipv4

ipv4: $(OBJS)
	$(CC) $(CFLAGS) $(SRCS) -o $(ipv4) -pthread

clean:
	rm -rf ${OBJS}