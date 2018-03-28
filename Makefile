.PHONY all:
all:
	gcc -Wall -D PART1 parts.c -o diskinfo
	#gcc -Wall -D PART2 parts.c -o disklist
	gcc -Wall -D PART3 parts.c -o diskget

.PHONY clean:
clean:
	-rm diskinfo disklist diskget
