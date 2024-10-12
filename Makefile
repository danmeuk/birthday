CFLAGS=		-Wall -Werror -pedantic -O2 -I/usr/local/include
LDFLAGS=	-L/usr/local/lib

all:		bd

bd:		bd.c
		clang ${CFLAGS} ${LDFLAGS} -o bd bd.c

clean:
		rm -f bd bd-static

install:	all
		install -b -m 0755 -o root -g wheel -s bd bd /usr/local/tools/

