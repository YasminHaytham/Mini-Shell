c= g++
cFlags= -Wall -g

myshell: command.o tokenizer.o
	$(c) $(cFlags) command.o tokenizer.o -o myshell

command.o: command.cc command.h tokenizer.h
	$(c) $(cFlags) -c command.cc

tokenizer.o: tokenizer.cc tokenizer.h
	$(c) $(cFlags) -c tokenizer.cc

clean:
	rm -f command.o tokenizer.o myshell

.PHONY: clean