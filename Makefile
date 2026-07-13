# Compilador y banderas estándar POSIX
CC = gcc
CFLAGS = -Wall -Wextra -pthread

TARGETS = servidor cliente

all: $(TARGETS)

servidor: servidor.c protocolo.h
	$(CC) $(CFLAGS) servidor.c -o servidor

cliente: cliente.c protocolo.h
	$(CC) $(CFLAGS) cliente.c -o cliente

clean:
	rm -f $(TARGETS)
