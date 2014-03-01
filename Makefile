all: bench

%: %.c
	gcc -Wall -lrt $< -o $@

%.html: %.md
	markdown $< >$@

