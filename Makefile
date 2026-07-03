CC ?= clang
SRC_DIR := src
BUILD_DIR := /tmp/sh-dev-bear-objects

CFLAGS := -std=c23 \
        -Wall -Wextra -Wpedantic \
        -Wno-nullability-extension \
        -I$(CURDIR)/$(SRC_DIR)

SOURCES := $(shell find $(SRC_DIR) -type f -name '*.c' | sort)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean print-sources

all: $(OBJECTS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	-$(CC) $(CFLAGS) -c $< -o $@

print-sources:
	@printf '%s\n' $(SOURCES)

clean:
	rm -rf $(BUILD_DIR)
