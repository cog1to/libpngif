SRC_DIR := src
SRC_FILES := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/gif/*.c)
UNAME := $(shell uname)
INCLUDE := -Isrc -Isrc/gif

ifeq ($(UNAME), Darwin)
	CFLAGS += -D OS_MAC -framework Cocoa -Isupport/
	IMAGE_VIEWER_TARGET += support/animator.m support/appdelegate.m support/image_viewer_mac.m
	LFLAGS += -Wl,-undefined -Wl,dynamic_lookup
else
	CFLAGS += -D OS_LINUX
	IMAGE_VIEWER_TARGET += support/animator.m support/appdelegate.m support/image_viewer_mac.m
	LFLAGS += -shared -fPIC -wl,-soname,libpngif.so.0
endif

all: $(SRC_FILES)
	make dynamic
	make tests

# Library

dynamic: $(SRC_FILES)
	gcc -Wall $(LFLAGS) -o bin/libpngif.so.0.1 $(INCLUDE) $(SRC_FILES)

# Tests

test_parsed: $(SRC_FILES) test/test_parsed.c
	gcc -Wall -o bin/test_parsed $(INCLUDE) $(SRC_FILES) test/test_parsed.c

test_codes: $(SRC_FILES) test/test_read_code.c
	gcc -Wall -o bin/test_codes $(INCLUDE) $(SRC_FILES) test/test_read_code.c

test_decoded: $(SRC_FILES) test/test_decoded.c
	gcc -Wall -o bin/test_decoded $(INCLUDE) $(CFLAGS) \
		$(SRC_FILES) test/test_decoded.c $(IMAGE_VIEWER_TARGET)

test_image_gif: $(SRC_FILES) test/test_image_gif.c
	gcc -Wall -o bin/test_image_gif $(INCLUDE) $(CFLAGS) \
		$(SRC_FILES) test/test_image_gif.c $(IMAGE_VIEWER_TARGET)

tests: $(SRC_FILES)
	make test_parsed
	make test_codes
	make test_decoded
	make test_image_gif

