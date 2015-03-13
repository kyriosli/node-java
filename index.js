console.time("init");
var bindings = require('./java.node');
//
initBinding();
console.timeEnd("init");
// creates an java vm.
// We do not reinitialize a new vm if one has already been created and we losts its reference.
// And note that once a vm is destroyed, createVm becomes unstable, its behavior is vendor-specific.
exports.createVm = function () {
    // TODO: options
    var options = []; //["-verbose:jni"];
    // Because JavaVMOption accepts zero-terminated cstring as its input, we must
    // make sure that the input strings does not contain \0.

    return new JavaVM(bindings.createVm(options.length, options.join('\0')));
};

function JavaVM(vm) {
    this.vm = vm;
}

var slice = Array.prototype.slice;

JavaVM.prototype = {
    findClass: function (name) {
        return new JavaClass(bindings.findClass(this.vm, name));
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

function JavaClass(handle) {
    this.handle = handle;

    this.methodCache = {};
}

var rType = /\[*(?:L[^;]+;|[ZBCSIFDJ])/g;

function invoker(isStatic, async) {

    return function (signature) {
        var method = findMethod(isStatic ? this : this.getClass(), signature, isStatic);
        var arr = slice.call(arguments, 1);

        var ret = (async ? bindings.invokeAsync : bindings.invoke)(this.handle, method, arr);
        if (ret !== null && typeof ret === 'object')
            return new JavaObject(ret, null);
        return ret;
    }
}

function findMethod(cls, signature, isStatic) {
    var methodCache = cls.methodCache;
    if (signature in methodCache)
        return methodCache[signature];

    return methodCache[signature] = bindings.findMethod(cls.handle, signature, isStatic)
}

JavaClass.prototype = {
    newInstance: function (signature) {
        
        var method = findMethod(this, arguments.length ? '<init>(' + signature + ')V' : '<init>()V', false),
            args = slice.call(arguments, 1);

        var ref = bindings.newInstance(this.handle, method, args);
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
        var cls = this.cls;
        if (!cls) {
            // TODO
            cls = this.cls = new JavaClass(bindings.getClass(this.handle));
        }
        //this.getClass = function() {return cls};
        return cls;
    },
    invoke: invoker(false, false),
    invokeAsync: invoker(false, true),
    asClass: function () {
        return new JavaClass(this.handle);
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
