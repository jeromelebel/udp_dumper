CC=gcc
CFLAGS=
LDFLAGS=-lstdc++
EXEC=udp_dumper
SRC = UDPClient.cpp main.cpp
OBJ = ${SRC:.cpp=.o}

all: ${OBJ}
	$(CC) -o $(EXEC) ${OBJ} $^ ${LDFLAGS}

main.o: main.h UDPClient.h
UDPClient.o: main.h UDPClient.h

.cpp.o:
	$(CC) -o $@ -c ${.IMPSRC} $(CFLAGS)

clean:
	@rm *.o
	@rm udp_dumper

