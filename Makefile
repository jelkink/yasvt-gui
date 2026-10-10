TARGET := yasvt-gui
SRC_DIR := src
BUILD_DIR := build

CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS  := -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags gtk4)
LDFLAGS := $(shell $(PKG_CONFIG) --libs gtk4)

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all run clean

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

run: all
	./$(BUILD_DIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJS:.o=.d)
