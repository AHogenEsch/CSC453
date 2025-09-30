CFLAGS = -Wall -Wextra -Werror -ansi -pedantic

# Link object file to create executable
pipeit: pipeit.o
	gcc -o pipeit pipeit.o

# Compile source file to object file
pipeit.o: pipeit.c 
	gcc -c pipeit.c -o pipeit.o

# Clean target to remove build artifacts
clean:
	rm -f pipeit pipeit.o