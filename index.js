console.time("init");
var bindings = require('./java.node');
//
initBinding();
console.timeEnd("init");
// creates an java vm.
// We do not reinitialize a new vm if one has already been created and we losts its reference.
// And note that once a vm is destroyed, createVm becomes unstable, its behavior is vendor-specific.
var instance = null;

exports.createVm = function () {
    if (instance)
        return instance;
    // Because JavaVMOption accepts zero-terminated cstring as its input, we must
    // make sure that the input strings does not contain \0.
    return instance = new JavaVM(bindings.createVm(arguments.length, Array.prototype.join.call(arguments, '\0')));
};

function JavaVM(vm) {
    this.vm = vm;
    this.classCache = {};
}

var slice = Array.prototype.slice;

JavaVM.prototype = {
    getClass: function (ptr, self) {
        var arr = bindings.getClass(ptr, this.classCache, self),
            className = arr[0],
            handle = arr[1];
        return handle ? this.classCache[className] = new JavaClass(className, handle) : this.classCache[className];
    },
    findClass: function (name) {
        var classCache = this.classCache;
        if (name in classCache) return classCache[name];

        return classCache[name] = new JavaClass(name, bindings.findClass(this.vm, name));
    },
    runMain: function (cls, args) {
        // cls is of form com/pkg/name/ClassName$SubClassName
        bindings.runMain(this.vm, cls, args);
    },
    runMainAsync: function (cls, args) {
        return bindings.runMain(this.vm, cls, args, true);
    },
    destroy: function () {
        bindings.dispose(this.vm);
        this.vm = null;
    }
};

function JavaClass(name, handle) {
    this.name = name;
    this.handle = handle;

    this.methodCache = {};
}

var rType = /\[*(?:L[^;]+;|[ZBCSIFDJ])/g;

function invoker(isStatic, async) {
    return async ? function (signature) {
        var method = findMethod(isStatic ? this : this.getClass(), signature, isStatic);

        return bindings.invokeAsync(this.handle, method, arguments).then(invokeFilter);
        // passing `arguments' into c++ code is the most efficient way
    } : function (signature) {
        var method = findMethod(isStatic ? this : this.getClass(), signature, isStatic);

        return invokeFilter(bindings.invoke(this.handle, method, arguments));
    }
}

function invokeFilter(ret) {
    if (ret !== null && typeof ret === 'object')
        return new JavaObject(ret, null);
    return ret;
}

function findMethod(cls, signature, isStatic) {
    var methodCache = cls.methodCache;
    if (signature in methodCache)
        return methodCache[signature];

    return methodCache[signature] = bindings.findMethod(cls.handle, signature, isStatic)
}

JavaClass.prototype = {
    newInstance: function (signature) {

        var method = findMethod(this, arguments.length ? '<init>(' + signature + ')V' : '<init>()V', false);

        var ref = bindings.newInstance(this.handle, method, arguments);
        return new JavaObject(ref, this);
    },
    invoke: invoker(true, false),
    invokeAsync: invoker(true, true),
    asObject: function () {
        return new JavaObject(this.handle, null);
    }
};


function parseArgs(args, types) {
    var ret = [];
    for (var i = 0, l = types.length; i < l; i++) {
        var val = args[i + 1];
        var type = types[i];
        if (type === 'L') { // object
            val = val && val.handle || null;
        } else if (type === '$') { // string
            // a java string maybe passed as a JavaObject
            if (val) {
                val = val.handle || String(val);
            } else if (val === undefined) {
                val = null;
            }
        }
        ret[i] = val;
    }
    return ret;
}

function JavaObject(handle, cls) {
    this.cls = cls;
    this.handle = handle;
}

JavaObject.prototype = {
    getClass: function () {
        return this.cls || (this.cls = instance.getClass(this.handle, false));
    },
    invoke: invoker(false, false),
    invokeAsync: invoker(false, true),
    asClass: function () {
        return instance.getClass(this.handle, true);
    },
    asObjectArray: function () {
        return new JavaObjectArray(this.handle);
    }
};

function JavaObjectArray(handle) {
}

function initBinding() {
    var java_home = process.env.JAVA_HOME,
        jre_home = process.env.JRE_HOME;
    if (java_home === undefined && jre_home === undefined) {
        throw new Error("neither JAVA_HOME nor JRE_HOME is found in environment variables");
    }
    var fs = require('fs');

    var platform = process.platform, arch = process.arch;

    var paths = [];

    switch (true) {
        case java_home && platform === "linux" && arch === "x64" && test(java_home + "/jre/lib/amd64/server/libjvm.so"):
        case jre_home && platform === "linux" && arch === "x64" && test(jre_home + "/lib/amd64/server/libjvm.so"):
            break;

        default:
            throw new Error("jvm shared library not found. paths tested are: \n  " + paths.join('\n  '));
    }


    function test(path) {
        if (fs.existsSync(path)) {
            bindings.link(path);
            return true;
        }
        paths.push(path);
    }

}
