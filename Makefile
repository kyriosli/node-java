MODULES=\
	java.o\
	java_invoke.o\
	java_exception.o\
	async.o\
	java_async.o

TESTS=\
	test/index\
	test/invoke

CPP=g++
TARGET=java.node

NODE_PREFIX=$(shell dirname $$(dirname $$(which node)))
UNAME=linux

COMPILER=$(CPP) -c -fPIC -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(UNAME) -I$(NODE_PREFIX)/include/node
LINKER=$(CPP) -shared

all: test

$(TARGET): $(MODULES)
	@echo 'linking $@'
	$(LINKER) $^ -o $@

%.o: %.cc java.h
	@echo 'compiling $@'
	$(COMPILER) -o $@ $<

test:  $(TESTS)


test/%:	$(TARGET)
	node $@.js

clean:
	rm -rf $(TARGET) *.o
