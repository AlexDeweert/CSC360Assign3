.PHONY all:
all:
	gcc -Wall -D PART1 parts.c -o diskinfo -lm
	gcc -Wall -D PART2 parts.c -o disklist -lm
	gcc -Wall -D PART3 parts.c -o diskget -lm
	gcc -Wall -D PART4 parts.c -o diskput -lm

.PHONY clean:
clean:
	-rm -rf *.o *.exe
