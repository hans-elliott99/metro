SRC_DIR := ./src
OBJ_DIR := ./obj
BIN_DIR := .
INCLUDE_DIR := ./src #eventually put headers in include/, in src for intellisense
TARGET := $(BIN_DIR)/main
TEST_DIR := ./tests

non_main := $(filter-out $(SRC_DIR)/main.c, $(wildcard $(SRC_DIR)/*.c))
SRC := $(non_main) $(SRC)/main.c
TEST_SRC := $(non_main) $(wildcard $(TEST_DIR)/*.c)
# $(info    TEST_SRC is $(TEST_SRC))
 
non_main_obj := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(non_main))
OBJ := $(non_main_obj) $(OBJ_DIR)/main.o
TEST_OBJ := $(non_main_obj) $(patsubst $(TEST_DIR)/%.c, $(OBJ_DIR)/%.o, $(wildcard $(TEST_DIR)/*.c))
# $(info    TEST_OBJ is $(TEST_OBJ))

CC := gcc
CPPFLAGS := -I$(INCLUDE_DIR) -MMD -MP #c preprocessor
CFLAGS := -Wall -Wextra -std=c11 -pedantic -ggdb
LDFLAGS := -Llib #linker flag
LDLIBS := -lm

.PHONY: default all clean test
.PRECIOUS: $(TARGET) $(OBJ)

default: $(TARGET)
all: default
test: $(BIN_DIR)/test

# Linking
$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: $(TEST_OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compilation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

## (test)
$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(TEST_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Other
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -f $(OBJ_DIR)/*.o
	rm -f $(OBJ_DIR)/*.d
	rm -f $(TARGET)
	rm -f $(BIN_DIR)/test

-include $(OBJ:.o=.d)

# I like this makefile: https://stackoverflow.com/questions/30573481/how-to-write-a-makefile-with-separate-source-and-header-directories
# Ideal testing setup: https://stackoverflow.com/questions/17896751/makefile-use-multiple-makefiles
