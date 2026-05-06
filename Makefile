CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude \
          $(shell sdl2-config --cflags)
LIBS    = $(shell sdl2-config --libs) -lSDL2_ttf
TARGET  = ekagra

SRC     = src/main.c \
          src/editor.c \
          src/render.c \
          src/sidebar.c \
          src/search.c \
          src/commands.c

OBJ     = $(SRC:.c=.o)

# ---- Targets ----

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
