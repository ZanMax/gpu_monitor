CC=gcc
CFLAGS=-fPIC -Wall -Wextra -O2
LDFLAGS=-shared -lnvidia-ml -lpci

TARGET=libgpumonitor.so
SRCS=gpu_monitor.c
OBJS=$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install:
	cp $(TARGET) /usr/local/lib/
	ldconfig

.PHONY: all clean install