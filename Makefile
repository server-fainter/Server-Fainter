# Makefile

CC = gcc
LDFLAGS = -lpthread -lssl -lcrypto

TARGET = server
SRCS = http_server.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $<

clean:
	rm -f $(TARGET) $(OBJS)
