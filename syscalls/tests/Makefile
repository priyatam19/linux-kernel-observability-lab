CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = test_encrypt

all: $(TARGET)

$(TARGET): test_s2_encrypt.c
	$(CC) $(CFLAGS) -o $(TARGET) test_s2_encrypt.c

clean:
	rm -f $(TARGET)
