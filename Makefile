CC = gcc
CFLAGS = -g -std=c99 -Wall -fsanitize=address,undefined

mysh: mysh.c
	gcc mysh.c -o mysh.o
	@echo "Run the program using ./mysh.o"


clean:
	rm -f *.o