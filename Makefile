CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror
CC = gcc
EXECS = 33sh 33noprompt

USE = sh.c jobs.c jobs.h

.PHONY: all clean

PROMPT = -DPROMPT

all: $(EXECS)

33sh: sh.c jobs.c jobs.h
	$(CC) $(CFLAGS) $(USE) -o $@ -DPROMPT
33noprompt: sh.c jobs.c jobs.h
	$(CC) $(CFLAGS) $(USE) -o $@
clean:
	rm -f $(EXECS)

