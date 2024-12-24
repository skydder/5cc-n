SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:%.c=target/%.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:%.c=target/%.exe)

CC=gcc

5cc:$(OBJS)
	$(CC) -o 5cc $(OBJS)

target/src/%.o: src/%.c
	$(CC) -c -o target/src/$*.o src/$*.c

target/test/%.exe: 5cc test/%.c
	$(CC) -o- -E -P -C test/$*.c | ./5cc -o target/test/$*.s -
	# $(CC) -o $@ target/test/$*.s -xc test/common

test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/test2.sh

clean:
	rm -f 5cc target/src/*.o  target/test/*.s target/test/*.exe

.PHONY: test clean 5cc