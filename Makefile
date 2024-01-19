PROGRAMS = server \
		   client

CFLAGS = -pthread


all: $(PROGRAMS)

server: server.c
	cc $(CFLAGS) $^ -o $@

client: client.c
	cc $(CFLAGS) $^ -o $@

rm:
	rm -f $(PROGRAMS)
