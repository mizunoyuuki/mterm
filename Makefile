CC = clang
CFLAGS = $(shell sdl2-config --cflags) $(shell pkg-config --cflags SDL2_ttf) -Wall -Wextra
LDFLAGS = $(shell sdl2-config --libs) $(shell pkg-config --libs SDL2_ttf) -lutil
SRCS = main.c pty.c render.c
HDRS = pty.h render.h
TARGET = myterm

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)
