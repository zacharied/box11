NAME = box11

CC = gcc
CFLAGS = -g -Wall -Wextra

INCLUDES = pango
LIBS = xcb xcb-xrm pangocairo
PKGCONF := `pkg-config --cflags $(INCLUDES) --libs $(LIBS)`

SOURCE_DIR = src
TARGET_DIR = build

SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
TARGET := $(TARGET_DIR)/$(NAME)

INSTALL_PATH ?= /usr/bin

$(NAME):
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(PKGCONF)

clean:
	rm -r $(TARGET_DIR) $(OUTPUT)

install:
	cp $(TARGET) $(INSTALL_PATH)

uninstall:
	rm $(INSTALL_PATH)/$(TARGET)

.PHONY: clean install uninstall
