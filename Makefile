override CFLAGS:=-O3 $(CFLAGS)
LDFLAGS=

sources=$(wildcard src/*.c)
objects=$(sources:%.c=%.o)

dhcp-server: $(objects)
	$(CC) $(LDFLAGS) -o $(@) $(^)

.c.o:
	$(CC) $(CFLAGS) -o $(@) -c $(<)

.PHONY: clean
clean:
	rm -f src/*.o dhcp-server
