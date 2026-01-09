MINIAUDIO_DIR := ext/miniaudio-0.11.23
MINIADUIO_SRC := $(MINIAUDIO_DIR)/src/miniaudio.c
MINIADUIO_OBJ := $(patsubst $(MINIAUDIO_DIR)/src/%.c,$(OBJ_DIR)/miniaudio/%.o,$(MINIADUIO_SRC))

MINIAUDIO_CFLAGS := -I$(MINIAUDIO_DIR)/include

OBJ += $(MINIADUIO_OBJ)
INC += -I$(MINIAUDIO_DIR)/include

$(OBJ_DIR)/miniaudio/%.o: $(MINIAUDIO_DIR)/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(MINIAUDIO_CFLAGS) -c $< -o $@
