BIN_NAME=aucont_start
MAIN_TARGET=$(BIN_NAME)

include ../Makefile.inc

C_FLAGS+=-I../libaucont/src
LD_FLAGS+=-L$(BIN_DIR) -static -laucont
BIN=$(BIN_DIR)/$(BIN_NAME)

$(MAIN_TARGET): $(OBJECTS)
	$(CC) $(C_FLAGS) -o $(BIN) $(OBJECTS) $(LD_FLAGS)
