MODULES=\
	java.o

CPP=g++
TARGET=java.node

NODE_PREFIX=$(shell dirname $$(dirname $$(which node)))
UNAME=linux
ARCH=amd64

COMPILER=$(CPP) -c -fPIC -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(UNAME) -I$(NODE_PREFIX)/include/node -DLD_LIB="\"$(JAVA_HOME)/jre/lib/amd64/server/libjvm.so\""
LINKER=$(CPP) -shared

all: test

$(TARGET): $(MODULES)
	@echo 'linking $@'
	$(LINKER) $^ -o $@

%.o: %.cc
	@echo 'compiling $@'
	$(COMPILER) -o $@ $^

test:  $(TARGET)
	node test

test/%:	$(TARGET)
	node $@.js

clean:
	rm -rf $(TARGET) *.o
