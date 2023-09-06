CC=cc
FLAGS=-std=c99 -Wall -Wextra -Werror -pedantic -g -Wno-use-after-free
TARGET=pm
FILES=main.c

default: $(clean) $(TARGET)
	cp $(TARGET) ~/.local/bin

all: $(TARGET)

$(TARGET): $(FILES)
	$(CC) $(FLAGS) $(FILES) -o $(TARGET)

clean:
	rm -f $(TARGET)
	rm -f ~/.local/bin/$(TARGET)
