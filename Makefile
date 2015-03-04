MODULES=\
	java.o

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

%.o: %.cc
	@echo 'compiling $@'
	$(COMPILER) -o $@ $^

test:  $(TESTS)


test/%:	$(TARGET)
	node $@.js

clean:
	rm -rf $(TARGET) *.o
