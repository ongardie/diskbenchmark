all: bench2

%: %.c
	gcc -Wall $< -o $@

%.html: %.md
	markdown $< >$@

