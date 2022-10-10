SRC_DIR := src
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)

test_parsed: $(SRC_FILES) test/test_parsed.c
	gcc -Wall -o bin/test_parsed -Isrc/ $(SRC_FILES) test/test_parsed.c

test_codes: $(SRC_FILES) test/test_read_code.c
	gcc -Wall -o bin/test_codes -Isrc/ $(SRC_FILES) test/test_read_code.c

test_decoded: $(SRC_FILES) test/test_decoded.c
	gcc -Wall -o bin/test_decoded -Isrc/ $(SRC_FILES) test/test_decoded.c

tests: $(SRC_FILES)
	make test_parsed
	make test_codes
	make test_decoded

