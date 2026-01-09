CLIENT_DIR := src/client
CLIENT_SRC := $(shell "$(FIND)" "$(CLIENT_DIR)" -name "*.c")
CLIENT_OBJ := $(patsubst $(CLIENT_DIR)/%.c,$(OBJ_DIR)/%.o,$(CLIENT_SRC))

OBJ += $(CLIENT_OBJ)

$(OBJ_DIR)/%.o: $(CLIENT_DIR)/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
