TARGET := yasvt-gui
SRC_DIR := src
BUILD_DIR := build

PREFIX ?= /usr/local
DESTDIR ?=
BINDIR := $(PREFIX)/bin
APPDIR := $(PREFIX)/share/applications

CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS  := -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags gtk4)
LDFLAGS := $(shell $(PKG_CONFIG) --libs gtk4)

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all run clean install uninstall

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

install: all
	install -Dm755 $(BUILD_DIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -d $(DESTDIR)$(APPDIR)
	printf '[Desktop Entry]\nType=Application\nName=YASVT\nExec=$(TARGET)\nTerminal=false\nCategories=Education;\n' > $(DESTDIR)$(APPDIR)/$(TARGET).desktop

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET) $(DESTDIR)$(APPDIR)/$(TARGET).desktop

-include $(OBJS:.o=.d)
