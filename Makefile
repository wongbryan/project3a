.SILENT: 

default: fs 
fs: 
	gcc -o lab3a lab3a.c -Wall -Wextra
dist: fs
	tar -czf lab3a-504744476.tar.gz lab3a-client.c ext2_fs.h Makefile README
clean: 
	rm -f *.tar.gz lab3a