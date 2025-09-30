CC = gcc
CFLAGS = -Wall -g -fpic
TEST_APP = tryme
LIB_PATH = ./lib64:~pn-cs453/Given/Asgn1/lib64

intel-all: lib/libmalloc.so lib64/libmalloc.so

lib/libmalloc.so: lib malloc32.o
	$(CC) $(CFLAGS) -m32 -shared -o $@ malloc32.o

lib64/libmalloc.so: lib64 malloc64.o
	$(CC) $(CFLAGS) -shared -o $@ malloc64.o

lib:
	mkdir lib

lib64:
	mkdir lib64

malloc32.o: malloc.c
	$(CC) $(CFLAGS) -m32 -c -I ~pn-cs453/Given/Asgn1/include -o malloc32.o malloc.c

malloc64.o: malloc.c
	$(CC) $(CFLAGS) -m64 -c -I ~pn-cs453/Given/Asgn1/include -o malloc64.o malloc.c


link:
	LD_LIBRARY_PATH=$(LIB_PATH)
	export LD_LIBRARY_PATH

debug:
	DEBUG_MALLOC=1
	export DEBUG_MALLOC


# Rule to compile the 64-bit test application, linking it against lib64/libmalloc.so
$(TEST_APP): $(TEST_APP).c lib64/libmalloc.so
	$(CC) $(CFLAGS) -o $@ $(TEST_APP).c -Llib64 -lmalloc \
	-L ~pn-cs453/Given/Asgn1/lib64 -lpp
	./$(TEST_APP)

# Clean target should also remove test artifacts and disable debug malloc
clean:
	unset DEBUG_MALLOC
	rm -f lib/libmalloc.so lib64/libmalloc.so malloc32.o malloc64.o $(TEST_APP)
	rmdir lib lib64 2>/dev/null || true