CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11

SRC_DIR = .
BUILD_DIR = build

SRCS = main.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

TARGET = $(BUILD_DIR)/ipv4-chat

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) -pthread

$(BUILD_DIR)/%.o: $(SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRCS) -o $(OBJS)

clean:
	rm -rf $(BUILD_DIR)