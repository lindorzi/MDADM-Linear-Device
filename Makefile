CC=gcc
CFLAGS=-c -Wall -I. -fpic -g -fbounds-check
LDFLAGS=-L.
LIBS=-lcrypto

OBJS=tester.o util.o mdadm.o cache.o net.o

%.o:	%.c %.h
	$(CC) $(CFLAGS) $< -o $@

tester:	$(OBJS) jbod-m1-a5-u20.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(OBJS) tester
