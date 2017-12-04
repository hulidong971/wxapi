all: httpd

httpd: httpd.c wxapi.c
	gcc -W -Wall -lpthread -o httpd httpd.c wxapi.c

clean:
	rm httpd
