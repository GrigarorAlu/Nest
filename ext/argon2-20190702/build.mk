ARGON2_DIR := ext/argon2-20190702
ARGON2_SRC := $(ARGON2_DIR)/src/argon2.c $(ARGON2_DIR)/src/blake2/blake2b.c $(ARGON2_DIR)/src/core.c \
    $(ARGON2_DIR)/src/encoding.c $(ARGON2_DIR)/src/opt.c $(ARGON2_DIR)/src/thread.c

ARGON2_CFLAGS := -I$(ARGON2_DIR)/include

OBJ += $(patsubst $(ARGON2_DIR)/src/%.c,$(OBJ_DIR)/argon2/%.o,$(ARGON2_SRC))
INC += -I$(ARGON2_DIR)/include

$(OBJ_DIR)/argon2/%.o: $(ARGON2_DIR)/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ARGON2_CFLAGS) -c $< -o $@
