CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = minishell
SOURCES = shell.c

all: $(TARGET)

$(TARGET): $(SOURCES) shell.h
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
