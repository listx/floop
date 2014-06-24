CC = gcc
#CC = clang
CFLAGS = -Wall -Wextra -Werror -O3
# CPU-specific optimization
CFLAGS += -march=native
# Use pipe instead of temporary files b/n various stages of compilation
CFLAGS += -pipe
# Comment to enable debugging macros.
CFLAGS += -DNDEBUG

LFLAGS += -pthread
OBJECTS = main.o random.o thread.o types.o

all: $(OBJECTS)
		$(CC) $(CFLAGS) $(OBJECTS) $(LFLAGS) -o floop

%.o: %.c
		$(CC) $(CFLAGS) -c $<

clean:
		$(RM) *.o floop

srclist:
	$(shell find -type f -regex ".*\.\(c\|h\)" > srclist)
