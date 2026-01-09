RAYLIB_DIR := ext/raylib-5.5
RAYLIB_SRC := $(RAYLIB_DIR)/src/rcore.c $(RAYLIB_DIR)/src/rshapes.c \
    $(RAYLIB_DIR)/src/rtext.c $(RAYLIB_DIR)/src/rtextures.c $(RAYLIB_DIR)/src/utils.c

RAYLIB_CFLAGS := -I$(RAYLIB_DIR)/src

ifeq ($(PLATFORM),windows)
    LDFLAGS += -lgdi32 -lopengl32 -mwindows
endif

ifeq ($(PLATFORM),android)
else
    RAYLIB_SRC += $(RAYLIB_DIR)/src/rglfw.c
    RAYLIB_CFLAGS += -DPLATFORM_DESKTOP_GLFW -DGRAPHICS_API_OPENGL_33 \
        -I$(RAYLIB_DIR)/src/external/glfw/include
endif

RAYLIB_OBJ := $(patsubst $(RAYLIB_DIR)/src/%.c,$(OBJ_DIR)/raylib/%.o,$(RAYLIB_SRC))

OBJ += $(RAYLIB_OBJ)
INC += -I$(RAYLIB_DIR)/src

$(OBJ_DIR)/raylib/%.o: $(RAYLIB_DIR)/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -c $< -o $@
