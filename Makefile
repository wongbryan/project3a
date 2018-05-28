#NAME: Steven La,Bryan Wong
#EMAIL: stevenla@g.ucla.edu,bryanw0ng@g.ucla.edu
#ID: 004781046,504744476

.SILENT:

default: fs 

fs: 
	gcc -Wall -Wextra -o lab3a lab3a.c

dist: fs
	tar -czf lab3a-504744476.tar.gz lab3a.c ext2_fs.h Makefile README

clean: 
	rm -f *.tar.gz lab3a