SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:%.c=target/%.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:%.c=target/%.exe)

CC=gcc

5cc:$(OBJS)
	$(CC) -o 5cc $(OBJS)

target:make-target.sh
	./make-target.sh

target/src/%.o: src/%.c target
	$(CC) -c -o target/src/$*.o src/$*.c

target/test/%.exe: 5cc test/%.c target
	$(CC) -o- -E -P -C test/$*.c | ./5cc -o target/test/$*.s -

test: $(TESTS)
	

clean:
	rm -f 5cc target/src/*.o  target/test/*.s target/test/*.exe

.PHONY: test clean target