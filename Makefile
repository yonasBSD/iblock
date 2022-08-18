CFLAGS  += -pedantic -Wall -Wextra -Wmissing-prototypes \
           -Werror -Wshadow -Wstrict-overflow -fno-strict-aliasing \
           -Wstrict-prototypes -Wwrite-strings \
		   -Os
all: iblock
	
iblock: main.c
	${CC} -o iblock main.c

clean:
	rm -f iblock

install: iblock
	install -o root -g wheel iblock ${PREFIX}/sbin/

test: clean iblock
	@printf "hello\n" | nc -4 localhost 666
	@printf "hello\n" | nc -6 localhost 666
