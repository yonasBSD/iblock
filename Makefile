all: iblock
	
iblock:
	${CC} -o iblock main.c

clean:
	rm -f iblock

test: clean iblock
	@printf "hello\n" | nc -4 localhost 1965
	@printf "hello\n" | nc -6 localhost 1965
