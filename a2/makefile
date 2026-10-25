# Builds ./shell in the project root. No CMake required.
#
#   make          build ./shell
#   make clean    remove ./shell and the object files

TARGET  := shell
SRC_DIR := src
OBJ_DIR := build/make

CC     ?= gcc
CFLAGS := -std=gnu2x -Wall -Wextra -O2

READLINE_CFLAGS := $(shell pkg-config --cflags readline 2>/dev/null)
READLINE_LIBS   := $(shell pkg-config --libs readline 2>/dev/null || echo -lreadline)

CPPFLAGS += $(READLINE_CFLAGS)
LDLIBS   += $(READLINE_LIBS)

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# -MMD -MP records header dependencies, so touching a header in src/include
# rebuilds every object that includes it.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	$(RM) -r $(OBJ_DIR) $(TARGET)
