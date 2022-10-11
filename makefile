SRC_DIR := src
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	CFLAGS += -D OS_MAC -framework Cocoa
	DECODED_TARGET += test/test_decoded.m
else
	CFLAGS += -D OS_LINUX
	DECODED_TARGET += test/test_decoded.c
endif

test_parsed: $(SRC_FILES) test/test_parsed.c
	gcc -Wall -o bin/test_parsed -Isrc/ $(SRC_FILES) test/test_parsed.c

test_codes: $(SRC_FILES) test/test_read_code.c
	gcc -Wall -o bin/test_codes -Isrc/ $(SRC_FILES) test/test_read_code.c

test_decoded: $(SRC_FILES) test/test_decoded.c
	gcc -Wall -o bin/test_decoded -Isrc/ $(CFLAGS) $(SRC_FILES) $(DECODED_TARGET)

tests: $(SRC_FILES)
	make test_parsed
	make test_codes
	make test_decoded

