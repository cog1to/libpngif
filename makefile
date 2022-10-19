SRC_DIR := src
OBJ_DIR := obj
SRC_FILES := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/gif/*.c)
OBJ := $(SRC_FILES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
UNAME := $(shell uname)
CFLAGS := -Isrc -Isrc/gif

ifeq ($(UNAME), Darwin)
	ADDCFLAGS += -framework Cocoa -Isupport/
	IMAGE_VIEWER_TARGET += support/animator.m support/appdelegate.m support/image_viewer_mac.m
	LFLAGS += -Wl,-undefined -Wl,dynamic_lookup
else
	ADDCFLAGS +=
	IMAGE_VIEWER_TARGET +=
	LFLAGS += -shared -fPIC -wl,-soname,libpngif.so.0
endif

all: $(SRC_FILES)
	make static
	make dynamic
	make tests

# Generic rule for .o files.
$(OBJ): $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	@mkdir -p obj/gif
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ)
	rm -rf bin/test_parsed bin/test_codes bin/test_decoded bin/test_image_gif bin/*.dSYM
	rm -f bin/libpngif.a bin/libpngif.so.0.1

static: $(OBJ)
	libtool -static -o bin/libpngif.a $(OBJ)

dynamic: $(SRC_FILES)
	gcc -Wall $(LFLAGS) -o bin/libpngif.so.0.1 $(OBJ)

# Tests

test_parsed: $(SRC_FILES) test/test_parsed.c
	gcc -Wall -o bin/test_parsed $(CFLAGS) $(SRC_FILES) test/test_parsed.c

test_codes: $(SRC_FILES) test/test_read_code.c
	gcc -Wall -o bin/test_codes $(CFLAGS) $(SRC_FILES) test/test_read_code.c

test_decoded: $(SRC_FILES) test/test_decoded.c
	gcc -Wall -o bin/test_decoded $(CFLAGS) $(ADDCFLAGS) \
		$(SRC_FILES) test/test_decoded.c $(IMAGE_VIEWER_TARGET)

test_image_gif: $(SRC_FILES) test/test_image_gif.c
	gcc -Wall -o bin/test_image_gif $(CFLAGS) $(ADDCFLAGS) \
		$(SRC_FILES) test/test_image_gif.c $(IMAGE_VIEWER_TARGET)

tests: $(SRC_FILES)
	make test_parsed
	make test_codes
	make test_decoded
	make test_image_gif

