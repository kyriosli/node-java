# java wrapper for Node.js

# build

build steps require JDK and Node.js installed, and `JAVA_HOME` environment must be set to the JDK root directory

linux users:
 
    npm install -g node-gyp # if you do not have node-gyp installed
    npm install git@github.com:kyriosli/node-java.git

# usage

    var vm = require('./node-java').createVm();
    vm.runMain("path/to/Main", ["string", "args"]);
    
## environment variables

  - JAVA_HOME
    default entry to build native module, and find jvm shared library at runtime.
  - JRE_HOME
    fallback entry to find jvm shared library.
  - NODE_JAVA_VERBOSE
    set this env to display more message and returns java stack trace when error occurs
    
    
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

  - 800,000 function calls with no parameters (`Object.hashValue()`) per second
  - 560,000 function calls with 8 parameters per second
  - 4,000,000 static field access(`Math.PI`) per second
  - 230,000 function calls from java to implemented methods with js


# apis

## module node-java

### createVm

    function createVm (string ...options)

Creates a jvm instance, or returns an instance already created.

Java vm options can be specified by arguments, such as `"-Xms256m"`

  - Returns: a instance of class `JavaVM`

## class JavaVM

### runMain

    function runMain(string className, optional string[] args)

Runs `public static void main(String[] args)` with specified class and arguments.
 
See [ClassNames](#classnames) for more details

### runMainAsync

    function runMainAsync(string className, optional string[] args)

Asynchronous version of `runMain` which returns a `Promise` and schedules main task in a thread pool

Warning: DO NOT run more than 4 thread blocking async processes at the same time, cause `node-java` uses Node.js's
uv_thread_pool to run those tasks, that shares the same thread pool limit with global module `fs`, so async file
operations won't be available till blocking state ends.
 
### findClass

    function findClass(string className)

  - Returns: a instance of class `JavaClasss`

Throws: class not found error

### implement

    function implement(optional string superClass, optional string[] interfaces, object methods)

Implement a java class with javascript methods. `interfaces` is list of java interfaces that the generated java class will
implement, and `methods` is map of methods that will be implemented.

For example:

    var vm = require('./node-java').createVm();
    var Runnable = vm.implement(['java/lang/Runnable'], {
        'run()V' : function() {
            console.log('Thread started');
        }
    });

    var thread = vm.findClass('java/lang/Thread').newInstance('Ljava/lang/Runnable;', Runnable.newInstance());
    thread.invoke('start()V');

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

    function newInstance(optional string argSignatures, mixed ...args)

  - Returns: a instance of class `JavaObject`

### invoke

    function invoke(string signature, mixed ...args)

### invokeAsync

    function invokeAsync(string signature, mixed ...args)

  - Returns: a promise

### get

    function get(string name, string type)

### set

    function set(string name, string type, mixed value)

## class JavaObject

### getClass

    function getClass()

  - Returns: a instance of class `JavaClass`

### invoke

    function invoke(string signature, mixed ...args)

### invokeAsync

    function invokeAsync(string signature, mixed ...args)

  - Returns: a promise

### get

    function get(string name, string type)

### set

    function set(string name, string type, mixed value)

### asClass

    function asClass()

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