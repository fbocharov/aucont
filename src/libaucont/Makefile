LIB_NAME=libaucont
MAIN_TARGET=$(LIB_NAME)

include ../Makefile.inc

AR=ar
AR_FLAGS=rcs

LIB_BIN=$(BIN_DIR)/$(LIB_NAME).a

$(MAIN_TARGET): $(OBJECTS)
	$(AR) $(AR_FLAGS) $(LIB_BIN) $(OBJECTS)
