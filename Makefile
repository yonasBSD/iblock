PREFIX =	/usr/local

CFLAGS =	-pedantic -Wall -Wextra -Wmissing-prototypes \
		-Werror -Wshadow -Wstrict-overflow -fno-strict-aliasing \
		-Wstrict-prototypes -Wwrite-strings \
		-Os


all: iblock
	
iblock: main.c
	${CC} ${CFLAGS} -o iblock main.c

clean:
	rm -f iblock

install: iblock
	install -o root -g wheel iblock ${PREFIX}/sbin/
	install -o root -g wheel iblock.rc /etc/rc.d/iblock

test: clean iblock
	@printf "hello\n" | nc -4 localhost 666
	@printf "hello\n" | nc -6 localhost 666
