NAME = xbox

CC = gcc
CFLAGS = -g -Wall -Wextra

LIBS = pango
LD_LIBS = xcb xcb-xrm pangocairo

SOURCE_DIR = src
TARGET_DIR = build

SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
HEADERS := $(wildcard $(SOURCE_DIR)/*.h)
OUTPUT := $(TARGET_DIR)/$(NAME)

INSTALL_PATH ?= /usr/bin

$(NAME):
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCES) $(HEADERS) `pkg-config --cflags $(LIBS) --libs $(LD_LIBS)`

clean:
	rm -r $(TARGET_DIR) $(OUTPUT)

install:
	cp $(OUTPUT) $(INSTALL_PATH)

uninstall:
	rm $(INSTALL_PATH)/$(NAME)

.PHONY: clean install uninstall
