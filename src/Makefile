SRCS=main.c tokenizer.c parser.c codegen.c util.c type.c
OBJS=$(SRCS:.c=.o)

5cc:$(OBJS)
	gcc -o 5cc $(OBJS)

$(OBJS): 5cc.h

test: 5cc
	./test.sh
tmp: tmp.s
	gcc -static -o tmp tmp.s tmp2.o

clean:
	rm -f 5cc *.o *~ tmp*

.PHONY: test clean tmp