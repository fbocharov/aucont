CC=gcc
C_FLAGS=-Wall -Werror -pedantic -g -std=gnu99
LD_FLAGS=

BIN_DIR=../../bin
OBJ_DIR=./obj
SRC_DIR=./src

SOURCES=$(wildcard $(SRC_DIR)/*.c)
OBJECTS=$(addprefix $(OBJ_DIR)/,$(notdir $(SOURCES:.c=.o)))

all: filestructure $(MAIN_TARGET)

filestructure:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $< $(C_FLAGS) -o $@

clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/*

.PHONY: clean all
