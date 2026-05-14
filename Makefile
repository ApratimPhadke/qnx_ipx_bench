CC           = qcc
TARGET_FLAGS = -Vgcc_ntox86_64
CFLAGS       = $(TARGET_FLAGS) -O2 -Wall

BIN = bin
SRC = src

all: $(BIN)/msg_server $(BIN)/msg_client $(BIN)/shm_writer $(BIN)/shm_reader

$(BIN)/msg_server: $(SRC)/msg_server.c
	mkdir -p $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/msg_client: $(SRC)/msg_client.c
	mkdir -p $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/shm_writer: $(SRC)/shm_writer.c
	mkdir -p $(BIN)
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/shm_reader: $(SRC)/shm_reader.c
	mkdir -p $(BIN)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN)