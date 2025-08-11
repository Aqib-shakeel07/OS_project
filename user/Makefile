CC := gcc
CFLAGS := -O2 -Wall -Wextra -pthread
LDFLAGS := -pthread

BIN := rw_test
SRC := rw_test.c
HDR := reader_writer_api.h

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(BIN)

