SRC_DIR := src
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	CFLAGS += -D OS_MAC -framework Cocoa -Isupport/
	IMAGE_VIEWER_TARGET += support/animator.m support/appdelegate.m support/image_viewer_mac.m
else
	CFLAGS += -D OS_LINUX
	IMAGE_VIEWER_TARGET += support/animator.m support/appdelegate.m support/image_viewer_mac.m
endif

test_parsed: $(SRC_FILES) test/test_parsed.c
	gcc -Wall -o bin/test_parsed -Isrc/ $(SRC_FILES) test/test_parsed.c

test_codes: $(SRC_FILES) test/test_read_code.c
	gcc -Wall -o bin/test_codes -Isrc/ $(SRC_FILES) test/test_read_code.c

test_decoded: $(SRC_FILES) test/test_decoded.c
	gcc --debug -Wall -o bin/test_decoded -Isrc/ $(CFLAGS) \
		$(SRC_FILES) test/test_decoded.c $(IMAGE_VIEWER_TARGET)

test_image_gif: $(SRC_FILES) test/test_image_gif.c
	gcc --debug -Wall -o bin/test_image_gif -Isrc/ $(CFLAGS) \
		$(SRC_FILES) test/test_image_gif.c $(IMAGE_VIEWER_TARGET)

tests: $(SRC_FILES)
	make test_parsed
	make test_codes
	make test_decoded
	make test_image_gif

