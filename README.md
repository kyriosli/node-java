# java wrapper for Node.js

# build

build steps require JDK and Node.js installed, and `JAVA_HOME` environment must be set to the JDK root directory

linux users:
 
    git clone git@github.com:kyriosli/node-java.git
    cd node-java
    make


# usage

    var vm = require('./node-java').createVm();
    vm.runMain("path/to/Main", ["string", "args"]);
    
# performance

tested on a single core linux server, with `iojs-1.1.0` and `jdk-1.8.0_25` 

    $ uname -a
    Linux ...... 3.16.0-4-amd64 #1 SMP Debian 3.16.7-ckt4-3 (2015-02-03) x86_64 GNU/Linux
    $ cat /proc/cpuinfo
    ...
    model name      : Intel(R) Xeon(R) CPU E5-2630 0 @ 2.30GHz
    ...
    $ node -v
    v1.1.0
    $ java -version
    java version "1.8.0_25"
    Java(TM) SE Runtime Environment (build 1.8.0_25-b17)
    Java HotSpot(TM) 64-Bit Server VM (build 25.25-b02, mixed mode)

  - 1,000,000 function calls (`Object.hashValue()`) per second
  - 4,000,000 static field access(`Math.PI`) per second


# apis

## module node-java

### function createVm (string ...options)

Creates a jvm instance, or returns an instance already created.

Java vm options can be specified by arguments, such as `"-Xms256m"`

  - Returns: a instance of class `JavaVM`

## class JavaVM

### function runMain(string className, optional string[] args)

Runs `public static void main(String[] args)` with specified class and arguments.
 
See [ClassNames](#classnames) for more details

### function runMainAsync(string className, optional string[] args)

Asynchronous version of `runMain` which returns a `Promise` and schedules main task in a thread pool

Warning: DO NOT run more than 4 thread blocking async processes at the same time, cause `node-java` uses Node.js's
uv_thread_pool to run those tasks, that shares the same thread pool limit with global module `fs`, so async file
operations won't be available till blocking state ends.
 
### function findClass(string className)

  - Returns: a instance of class `JavaClasss`

Throws: class not found error

## class JavaClass
 
### function newInstance(optional string argSignatures, mixed ...args)

  - Returns: a instance of class `JavaObject`

### function invoke(string signature, mixed ...args)

### function invokeAsync(string signature, mixed ...args)

  - Returns: a promise

### function get(string name, string type)

### function set(string name, string type, mixed value)

## class JavaObject

### function getClass()

  - Returns: a instance of class `JavaClass`

### function invoke(string signature, mixed ...args)

### function invokeAsync(string signature, mixed ...args)

  - Returns: a promise

### function get(string name, string type)

### function set(string name, string type, mixed value)

### function asClass()

  - Returns: a instance of class `JavaClass`
 
# ClassNames

ClassName is of form `package/path/to/Name`, and inner class's name is attached to its owner className with `$`, like:

    package/path/to/Outer$Inner

# Signatures

Method signature is defined as `name(argument types)return type`, where argument types is list of type signatures, and
  return type is single type signature. For example:

    main([Ljava/lang/String;)V
    toString()Ljava/lang/String;
    equals(Ljava/lang/Object;)Z

Type signature is defined as `[type signature | Z | B | S | C | I | F | D | J | LClassName;`.

See [JNI API Reference](http://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/types.html#type_signatures) for more details.


# TODO list:

  - array support
    - create primitive/object arrays
    - access primitive/object arrays
  - port to windows/osx
  - method proxy