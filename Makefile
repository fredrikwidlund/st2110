PROG   = main
OBJS   = main.o reader.o rtp.o packet.o frame.o
CFLAGS = -Wall -O3 -g -std=gnu11 -flto -fuse-linker-plugin
LDADD  = -ldynamic -lreactor

$(PROG): $(OBJS)
	$(CC) $(CFLAGS)   -o $@ $^ $(LDADD)

clean:
	rm $(PROG) $(OBJS)
