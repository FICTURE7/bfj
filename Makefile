CC=gcc
CFLAGS=-Wall -O3

SRC=bfj.c
BIN=bfj

.PHONY: test

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

test: $(BIN)
	./$(BIN) tests/hanoi.bf # && ndisasm -b 64 dump.bin

clean:
	rm -f $(BIN)
