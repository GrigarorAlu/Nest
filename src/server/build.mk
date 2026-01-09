SERVER_DIR := $(SRC_DIR)/server
SERVER_SRC := $(shell "$(FIND)" "$(SERVER_DIR)" -name "*.c")
SERVER_OBJ := $(patsubst $(SERVER_DIR)/%.c,$(OBJ_DIR)/%.o,$(SERVER_SRC))

OBJ += $(SERVER_OBJ)

$(OBJ_DIR)/%.o: $(SERVER_DIR)/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
