all: bench

%: %.c
	gcc -Wall $< -o $@

%.html: %.md
	markdown $< >$@

