ALL:
	gcc -rdynamic $$( pkg-config --cflags gtk+-3.0 ) -I ./ -o mp730424 app.c communication.c -lm $$( pkg-config --libs gtk+-3.0 )