.PHONY all:
all:
	gcc -Wall -D PART1 parts.c -o diskinfo
	gcc -Wall -D PART2 parts.c -o disklist

.PHONY clean:
clean:
	-rm diskinfo disklist
