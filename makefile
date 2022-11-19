SRC_DIR := src
OBJ_DIR := obj
SRC_FILES := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/gif/*.c) $(wildcard $(SRC_DIR)/png/*.c)
OBJ := $(SRC_FILES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
UNAME := $(shell uname)
CFLAGS := -Isrc -Isrc/gif -Isrc/png
LDFLAGS := -lz -lm

ifeq ($(UNAME), Darwin)
	ADDCFLAGS += -framework Cocoa -Isupport/
	IMAGE_VIEWER_TARGET += support/animator.m support/appdelegate.m support/image_viewer_mac.m
	LFLAGS += -Wl,-undefined -Wl,dynamic_lookup
else
	ADDCFLAGS += -lX11 -Isupport/
	IMAGE_VIEWER_TARGET += support/image_viewer_linux.c
	LFLAGS += -shared -fPIC -wl,-soname,libpngif.so.0
endif

all: $(SRC_FILES)
	make static
	make dynamic
	make tests

# Generic rule for .o files.
$(OBJ): $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	@mkdir -p obj/gif
	@mkdir -p obj/png
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ)
	rm -rf bin/test_gif_parsed bin/test_gif_codes bin/test_gif_decoded bin/test_gif_image \
		bin/test_png_parsed bin/test_png_decoded bin/test_png_image bin/test_png_chunks \
		bin/*.dSYM
	rm -f bin/libpngif.a bin/libpngif.so.0.1

static: $(OBJ)
	@mkdir -p bin
	ar r bin/libpngif.a $(OBJ)
	ranlib bin/libpngif.a

dynamic: $(SRC_FILES)
	@mkdir -p bin
	gcc -Wall $(LFLAGS) -o bin/libpngif.so.0.1 $(OBJ)

# Tests - GIF

test_setup:
	@mkdir -p bin

test_gif_parsed: $(SRC_FILES) test/test_gif_parsed.c
	make test_setup
	gcc -Wall -o bin/test_gif_parsed $(LDFLAGS) $(CFLAGS) $(SRC_FILES) test/test_gif_parsed.c

test_gif_codes: $(SRC_FILES) test/test_read_code.c
	make test_setup
	gcc -Wall -o bin/test_gif_codes $(LDFLAGS) $(CFLAGS) $(SRC_FILES) test/test_read_code.c

test_gif_decoded: $(SRC_FILES) test/test_gif_decoded.c
	make test_setup
	gcc -Wall -o bin/test_gif_decoded $(LDFLAGS) $(CFLAGS) $(ADDCFLAGS) \
		$(SRC_FILES) test/test_gif_decoded.c $(IMAGE_VIEWER_TARGET)

test_gif_image: $(SRC_FILES) test/test_gif_image.c
	make test_setup
	gcc -Wall -o bin/test_gif_image $(LDFLAGS) $(CFLAGS) $(ADDCFLAGS) \
		$(SRC_FILES) test/test_gif_image.c $(IMAGE_VIEWER_TARGET)

# Tests - PNG

test_png_chunks: $(SRC_FILES) test/test_png_chunks.c
	make test_setup
	gcc -Wall -o bin/test_png_chunks $(LDFLAGS) $(CFLAGS) $(SRC_FILES) test/test_png_chunks.c

test_png_parsed: $(SRC_FILES) test/test_png_parsed.c
	make test_setup
	gcc -Wall -o bin/test_png_parsed $(LDFLAGS) $(CFLAGS) $(SRC_FILES) test/test_png_parsed.c

test_png_decoded: $(SRC_FILES) test/test_png_decoded.c
	make test_setup
	gcc -Wall -o bin/test_png_decoded $(LDFLAGS) $(CFLAGS) $(ADDCFLAGS) \
		$(SRC_FILES) test/test_png_decoded.c $(IMAGE_VIEWER_TARGET)

test_png_image: $(SRC_FILES) test/test_png_image.c
	make test_setup
	gcc -Wall -o bin/test_png_image $(LDFLAGS) $(CFLAGS) $(ADDCFLAGS) \
		$(SRC_FILES) test/test_png_image.c $(IMAGE_VIEWER_TARGET)

test_image: $(SRC_FILES) test/test_image_viewer.c
	make test_setup
	gcc -Wall -o bin/test_image_viewer $(LDFLAGS) $(CFLAGS) $(ADDCFLAGS) \
		$(SRC_FILES) test/test_image_viewer.c $(IMAGE_VIEWER_TARGET)

tests: $(SRC_FILES)
	make test_gif_parsed
	make test_gif_codes
	make test_gif_decoded
	make test_gif_image
	make test_png_chunks
	make test_png_parsed
	make test_png_decoded
	make test_png_image
	make test_image

