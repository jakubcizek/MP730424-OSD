ALL:
	gcc -rdynamic $$( pkg-config --cflags gtk+-3.0 ) -o app app.c -lm $$( pkg-config --libs gtk+-3.0 )