SRCS=main.c tokenizer.c parser.c codegen.c util.c type.c
OBJS=$(SRCS:.c=.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:.c=.exe)

5cc:$(OBJS)
	gcc -o 5cc $(OBJS)

$(OBJS): 5cc.h


test/%.exe: 5cc test/%.c
	$(CC) -o- -E -P -C test/$*.c | ./5cc -o test/$*.s -
	$(CC) -o $@ test/$*.s -xc test/common

test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/test2.sh


clean:
	rm -f 5cc *.o *~ tmp* test/*.s test/*.exe

.PHONY: test clean