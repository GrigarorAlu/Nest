ENET_DIR := ext/enet-1.3.18
ENET_SRC := $(shell "$(FIND)" "$(ENET_DIR)/src" -name "*.c")
ENET_OBJ := $(patsubst $(ENET_DIR)/src/%.c,$(OBJ_DIR)/enet/%.o,$(ENET_SRC))

OBJ += $(ENET_OBJ)
INC += -I$(ENET_DIR)/include

$(OBJ_DIR)/enet/%.o: $(ENET_DIR)/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(ENET_DIR)/include -c $< -o $@
