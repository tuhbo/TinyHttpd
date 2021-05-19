LIBS += -lpthread
CFLAGS += -g -Wall
httpd:httpd.c
	gcc $(CFLAGS) -o $@ $< $(LIBS)
all:httpd
clean:
	rm httpd