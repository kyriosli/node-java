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
    
# apis

## module node-java

### function createVm (string ...options)

Creates a jvm instance, or returns an instance already created.

Java vm options can be specified by arguments, such as `"-Xms256m"`

  - Returns: a instance of class `JavaVM`

## class JavaVM

### function runMain(string className, optional string[] args)

Runs `public static void main(String[] args)` with specified class and arguments.
 
See [ClassNames](ClassNames) for more details

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

## class JavaObject

### function getClass()

  - Returns: a instance of class `JavaClass`

### function invoke(string signature, mixed ...args)

### function invokeAsync(string signature, mixed ...args)

  - Returns: a promise

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