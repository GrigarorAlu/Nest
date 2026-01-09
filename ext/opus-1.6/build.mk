OPUS_DIR := ext/opus-1.6

OPUS_SRC := $(OPUS_DIR)/src/extensions.c $(OPUS_DIR)/src/analysis.c $(OPUS_DIR)/src/mlp.c $(OPUS_DIR)/src/mlp_data.c \
    $(OPUS_DIR)/src/opus.c $(OPUS_DIR)/src/opus_decoder.c $(OPUS_DIR)/src/opus_encoder.c \
    $(OPUS_DIR)/src/repacketizer.c $(OPUS_DIR)/celt/bands.c $(OPUS_DIR)/celt/celt.c $(OPUS_DIR)/celt/celt_decoder.c \
    $(OPUS_DIR)/celt/celt_encoder.c $(OPUS_DIR)/celt/celt_lpc.c $(OPUS_DIR)/celt/cwrs.c $(OPUS_DIR)/celt/entcode.c \
    $(OPUS_DIR)/celt/entdec.c $(OPUS_DIR)/celt/entenc.c $(OPUS_DIR)/celt/kiss_fft.c $(OPUS_DIR)/celt/laplace.c \
    $(OPUS_DIR)/celt/mathops.c $(OPUS_DIR)/celt/mdct.c $(OPUS_DIR)/celt/modes.c $(OPUS_DIR)/celt/pitch.c \
    $(OPUS_DIR)/celt/quant_bands.c $(OPUS_DIR)/celt/rate.c $(OPUS_DIR)/celt/vq.c \
    $(shell "$(FIND)" "$(OPUS_DIR)/silk" -maxdepth 1 -name "*.c") \
    $(shell "$(FIND)" "$(OPUS_DIR)/silk/float" -maxdepth 1 -name "*.c")
    
OPUS_CFLAGS := -DOPUS_BUILD -DVAR_ARRAYS -O3 \
    -I$(OPUS_DIR)/celt -I$(OPUS_DIR)/silk -I$(OPUS_DIR)/silk/float -I$(OPUS_DIR)/include

OBJ += $(patsubst $(OPUS_DIR)/%.c,$(OBJ_DIR)/opus/%.o,$(OPUS_SRC))
INC += -I$(OPUS_DIR)/include

$(OBJ_DIR)/opus/%.o: $(OPUS_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPUS_CFLAGS) -c $< -o $@
