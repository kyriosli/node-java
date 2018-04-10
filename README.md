# java wrapper for Node.js

This module supplies a low-level implemention to create and interact with a java virtual machine within a node.js program. The interface is
such ugly that you should not use it directly in your program, a package that supplies a pure javascript style appearance should be used.

Currently we support only linux and osx, and windows support is on progress.


# Getting started

## Installing dependencies

As we were not supplying binary module files, you need to build it yourself. You need to install JDK and Node.JS as well as a c++ compiler.

For linux users: you should set a environment variable named `JAVA_HOME` that points to your JDK installation, such as
```sh
export JAVA_HOME=/usr/share/java/default-jdk
```
We have tested on g++-4.9, and should be working on 4.4+. If you run into any problem, please contact us by issuing a bug, and be kind enough to post your error message.

For osx users: we usually find your JDK by running `/usr/libexec/java_home`, but if we cannot find it, or you want to use a specific JDK installation, you should also supply the `JAVA_HOME` variable as we concerned before.

You can install a command line compiler by running `xcode-select --install` in your terminal.

## Building
 ```sh
(sudo) npm install -g node-gyp # if you do not have node-gyp installed
npm install nan # if you do not have nan installed
npm install kyriosli/node-java
```

## Running
```js
var vm = require('node-java').createVm();
vm.runMain("path/to/Main", ["string", "args"]);
```
At run time, you do not need a full JDK installation, but at least a JRE should be installed. The following environment variables will be used to check the java installation:

  - JAVA_HOME
    default entry to find jvm shared library at runtime. Can be ignored in osx (`/usr/libexec/java_home` is used)
  - JRE_HOME
    fallback entry to find jvm shared library if JAVA_HOME is not set
  - NODE_JAVA_VERBOSE
    gives more information about dynamic library loading. Also gives full java stack trace when an java exception has occurred and caught.
    
# performance

tested on a macbook air 2004 with `node-0.12.5` and `jdk-1.8.0_25` with power supply attached and energy saving disabled

```sh
$ uname -a
Darwin ... 14.5.0 Darwin Kernel Version 14.5.0: Wed Jul 29 02:26:53 PDT 2015; root:xnu-2782.40.9~1/RELEASE_X86_64 x86_64
$ sysctl -n machdep.cpu.brand_string
Intel(R) Core(TM) i5-4260U CPU @ 1.40GHz
```
**Getting class/object fields**

This metric shows the time cost of accessing a class's static field, or an object's member field. We choose `java.lang.Math.PI` as the example, the result is:

> get java.lang.Math.PI (100w): 590ms

**Invoking methods**

We tested the speed of method invocation in three situations:

  - calling java method with no arguments from javascript
  - calling java method with 8 arguments from javascript
  - calling java method that is implemented with javascript from java

The result is:

> invoke method with no args (100w): 1069ms
> 
> invoke method with 8 args (100w): 1596ms
> 
> invoke implemented method (100w): 2443ms

So we have near 170w times of field access, or near 93w times of method invocation from javascript to java, or near 41w times of method invocation from java to javascript, with a low voltage CPU.
  
# apis

## module node-java

### createVm
```js
function createVm (string ...options)
```
Creates a jvm instance, or returns an instance already created.

Java vm options can be specified by arguments, such as `"-Xms256m"`

  - Returns: a instance of class `JavaVM`

## class JavaVM

### runMain
```
function runMain(string className, optional string[] args)
```
Runs `public static void main(String[] args)` with specified class and arguments.
 
See [ClassNames](#classnames) for more details

### runMainAsync
```js
function runMainAsync(string className, optional string[] args)
```
Asynchronous version of `runMain` which returns a `Promise` and schedules main task in a thread pool

Warning: DO NOT run more than 4 thread blocking async processes at the same time, cause `node-java` uses Node.js's
uv_thread_pool to run those tasks, that shares the same thread pool limit with global module `fs`, so async file
operations won't be available till blocking state ends.
 
### findClass
```js
function findClass(string className)
```

  - Returns: a instance of class `JavaClasss`
  - Throws: class not found error

### implement
```js
function implement(optional string superClass, optional string[] interfaces, object methods)
```

Implement a java class with javascript methods. `interfaces` is list of java interfaces that the generated java class will
implement, and `methods` is map of methods that will be implemented.

For example:
```js
var vm = require('./node-java').createVm();
var Runnable = vm.implement(['java/lang/Runnable'], {
    'run()V' : function() {
        console.log('Thread started');
    }
});

var thread = vm.findClass('java/lang/Thread').newInstance('Ljava/lang/Runnable;', Runnable.newInstance());
thread.invoke('start()V');
```

  1. Note that, the implemented method calls made from inside or outside the Node.JS's main message loop is quite different.
If a method call is made inside the main loop, we can execute javascript codes immediately. However, if a method is called
from another thread(just like the example above), the thread will wait for the main loop to be idle, and when js code
executes completed, the thread is waked up.

  2. If a javascript error throws without caught, a `java/lang/RuntimeException` is thrown to the java environment.

  3. the default value for `superClass` is `java/lang/Object`, if you specify another class name, make sure it has a default
constructor.


  - Returns: a `JavaClass` instance


## class JavaClass
 
### newInstance
```js
function newInstance(optional string argSignatures, mixed ...args)
```

  - Returns: a instance of class `JavaObject`

### invoke
```js
function invoke(string signature, mixed ...args)
```

### invokeAsync
```js
function invokeAsync(string signature, mixed ...args)
```js

  - Returns: a promise

### get
```js
function get(string name, string type)
```

### set
```js
function set(string name, string type, mixed value)
```

## class JavaObject

### getClass
```js
function getClass()
```

  - Returns: a instance of class `JavaClass`

### invoke
```js
function invoke(string signature, mixed ...args)
```

### invokeAsync
```js
function invokeAsync(string signature, mixed ...args)
```

  - Returns: a promise

### get
```js
function get(string name, string type)
```

### set
```js
function set(string name, string type, mixed value)
```

### asClass
```js
function asClass()
```

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
  - port to windows
