CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11 -I. -Ifrontend -Ibackend
LDFLAGS = -lws2_32 -lwininet

TARGET = plagiarism_detector.exe

SRCS = main.c frontend/frontend.c backend/backend.c backend/miniz.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo [BUILD SUCCESS] Binary built: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo [CLEAN] Removed object files and binary.

.PHONY: all clean
