SQLITE_DIR := ext/sqlite-3.51.1
SQLITE_SRC := $(SQLITE_DIR)/src/sqlite3.c
SQLITE_OBJ := $(patsubst $(SQLITE_DIR)/src/%.c,$(OBJ_DIR)/sqlite/%.o,$(SQLITE_SRC))

SQLITE_CFLAGS := -I$(SQLITE_DIR)/include

OBJ += $(SQLITE_OBJ)
INC += -I$(SQLITE_DIR)/include

$(OBJ_DIR)/sqlite/%.o: $(SQLITE_DIR)/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SQLITE_CFLAGS) -c $< -o $@
