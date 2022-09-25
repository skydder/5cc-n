FILE_NAMES=main.c tokenizer.c parser.c codegen.c util.c type.c
SRCS=$(wildcard $(FILE_NAMES:%.c=src/%.c))
OBJS=$(SRCS:%.c=target/%.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:%.c=target/%.exe)

# $(OBJS):src/5cc.h $(SRCS)
# 	$(CC) -o $(OBJS) $(SRCS)


5cc:objs
	$(CC) -o 5cc $(OBJS)

objs:$(SRCS)
	for i in $^; do (echo target/$$i | sed 's/c$$/o/') | xargs  $(CC) -c  $$i -o; done;

target/test/%.exe: 5cc test/%.c
	$(CC) -o- -E -P -C test/$*.c | ./5cc -o target/test/$*.s -
	$(CC) -o $@ target/test/$*.s -xc test/common

test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/test2.sh

clean:
	rm -f 5cc target/src/*.o  target/test/*.s target/test/*.exe

.PHONY: test clean