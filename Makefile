CC=gcc
CCFLAGS=-std=c17 -Wall -Wextra -lcurl -lcjson
TARGET=weather-wave

SRC=main.c
OBJ=$(SRC:.c=.o)

$(TARGET): $(OBJ)
	$(CC) $(CCFLAGS) -o $(TARGET) $(OBJ)
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)