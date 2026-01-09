# Makefile

MODULE := client
PLATFORM := windows

TARGET := nest

WINDOWS_CC := gcc
UNIX_CC := zig
ANDROID_CC := clang

WINDOWS_CFLAGS :=
UNIX_CFLAGS :=
ANDROID_CFLAGS :=

WINDOWS_LDFLAGS := -lws2_32 -lwinmm
UNIX_LDFLAGS :=
ANDROID_LDFLAGS :=

FIND := D:/Devkit/bin/find

CFLAGS := -O3
LDFLAGS :=

ifeq ($(PLATFORM),windows)
    TARGET := $(TARGET).exe
    CC := $(WINDOWS_CC)
    CFLAGS += $(WINDOWS_CFLAGS)
    LDFLAGS += $(WINDOWS_LDFLAGS)
else ifeq ($(PLATFORM),unix)
    CC := $(UNIX_CC)
    CFLAGS += $(UNIX_CFLAGS)
    LDFLAGS += $(UNIX_LDFLAGS)
else ifeq ($(PLATFORM),android)
    CC := $(ANDROID_CC)
    CFLAGS += $(ANDROID_CFLAGS)
    LDFLAGS += $(ANDROID_LDFLAGS)
else
	$(error Unknown platform)
endif

SRC_DIR := src
OBJ_DIR := build/$(MODULE)/$(PLATFORM)
BIN_DIR := bin/$(MODULE)/$(PLATFORM)
RES_SRC := res
RES_DST := $(BIN_DIR)/res

.PHONY: all clean run

all: $(BIN_DIR)/$(TARGET)

SRC := $(shell "$(FIND)" "$(SRC_DIR)" -maxdepth 1 -name "*.c")
OBJ := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

CFLAGS += -MMD -MP
-include $(OBJ:.o=.d)

include ext/enet-1.3.18/build.mk
include ext/sqlite-3.51.1/build.mk

ifeq ($(MODULE),server)
    include src/server/build.mk
    include ext/argon2-20190702/build.mk 
else ifeq ($(MODULE),client)
    include src/client/build.mk
    include ext/miniaudio-0.11.23/build.mk
    include ext/opus-1.6/build.mk
    include ext/raylib-5.5/build.mk
    all: resources
else
    $(error Unknown platform)
endif

$(BIN_DIR)/$(TARGET): $(OBJ)
	mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

resources:
	mkdir -p $(RES_DST)
	cp -r $(RES_SRC)/* $(RES_DST)/

clean:
	rm -rf build bin

run: $(BIN_DIR)/$(TARGET)
	cd $(BIN_DIR) && ./$(TARGET)
