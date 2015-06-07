//console.time("init");
var bindings = require('./build/Release/java.node');
//
initBinding();
//console.timeEnd("init");
// creates an java vm.
// We do not reinitialize a new vm if one has already been created and we losts its reference.
// And note that once a vm is destroyed, createVm becomes unstable, its behavior is vendor-specific.
var instance = null;

exports.createVm = function () {
    if (instance)
        return instance;
    // Because JavaVMOption accepts zero-terminated cstring as its input, we must
    // make sure that the input strings does not contain \0.
    instance = new JavaVM(bindings.createVm(arguments.length, Array.prototype.join.call(arguments, '\0')));
    bindings[0] = function (clsName, method) {
        //console.log('invoke', clsName, method);
        return instance.classCache[clsName].methods[method].apply(null, Array.prototype.slice.call(arguments, 2));
    };
    return instance;
};

function JavaVM(vm) {
    this.vm = vm;
    this.classCache = {};
}

var nextImplId = 0;

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
    implement: function (parentName, interfaces, methods) {
        if (typeof parentName === 'object') {
            if (parentName instanceof Array) {
                methods = interfaces;
                interfaces = parentName;
            } else {
                methods = parentName;
                interfaces = [];
            }
            parentName = 'java/lang/Object';
        } else if (!(interfaces instanceof Array)) {
            methods = interfaces;
            interfaces = [];
        }

        var name = 'kyrios/Impl' + (nextImplId++).toString(36),
            impl = require('./implement').build(name, parentName, interfaces, Object.keys(methods));

        require('fs').writeFileSync(name + '.class', new Buffer(new Uint8Array(impl.buffer)));

        var handle = bindings.defineClass(this.vm, name, impl.buffer, impl.natives),
            ret = instance.classCache[name] = new JavaClass(name, handle);
        ret.methods = methods;

        return ret;
    },
    destroy: function () {
        bindings.dispose(this.vm);
        this.vm = null;
    }
};

function JavaClass(name, handle) {
    this.name = name;
    this.handle = handle;

    this.methodCache = [{}, {}];
    this.fieldCache = [{}, {}];
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
    },
    get: function (name, type) {
        var field = findField(this, name, type, true);
        return invokeFilter(bindings.get(this.handle, field));
    },
    set: function (name, type, value) {
        var field = findField(this, name, type, true);
        bindings.set(this.handle, field, value);
    }
};

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
    var methodCache = cls.methodCache[+isStatic];
    if (signature in methodCache)
        return methodCache[signature];

    return methodCache[signature] = bindings.findMethod(cls.handle, signature, isStatic)
}

function findField(cls, name, type, isStatic) {
    var fieldCache = cls.fieldCache[+isStatic],
        signature = name + ':' + type;
    if (signature in fieldCache)
        return fieldCache[signature];

    return fieldCache[signature] = bindings.findField(cls.handle, name, type, isStatic);
}

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
    get: function (name, type) {
        var field = findField(this.getClass(), name, type, false);
        return invokeFilter(bindings.get(this.handle, field));
    },
    set: function (name, type, value) {
        var field = findField(this.getClass(), name, type, false);
        bindings.set(this.handle, field, value);
    },
    asClass: function () {
        return instance.getClass(this.handle, true);
    },
    asObjectArray: function () {
        return new JavaObjectArray(this.handle);
    },
    asBooleanArray: function () {
        return new PrimitiveArray(this.handle, 'Z')
    },
    asByteArray: function () {
        return new PrimitiveArray(this.handle, 'B')
    },
    asShortArray: function () {
        return new PrimitiveArray(this.handle, 'S')
    },
    asCharArray: function () {
        return new PrimitiveArray(this.handle, 'C')
    },
    asIntArray: function () {
        return new PrimitiveArray(this.handle, 'I')
    },
    asFloatArray: function () {
        return new PrimitiveArray(this.handle, 'F')
    },
    asLongArray: function () {
        return new PrimitiveArray(this.handle, 'J')
    },
    asDoubleArray: function () {
        return new PrimitiveArray(this.handle, 'D')
    }
};

function PrimitiveArray(handle, type) {
    this.handle = handle;
    this.type = type.charCodeAt(0);
}

PrimitiveArray.prototype = {
    get length() {
        return bindings.getArrayLength(this.handle);
    },
    getRegion: function (start, end) {
        return bindings.getPrimitiveArrayRegion(this.handle, start, end, this.type);
    },
    setRegion: function (buf, start, end) {
        return bindings.setPrimitiveArrayRegion(this.handle, start, end, this.type, buf);
    }
};


function JavaObjectArray(handle) {
    this.handle = handle;
}

JavaObjectArray.prototype = {
    get length() {
        return bindings.getArrayLength(this.handle);
    },
    get: function (index) {
        var handle = bindings.getObjectArrayItem(this.handle, index);
        if (handle === null) {
            return null;
        }
        return new JavaObject(handle, null);
    },
    set: function (index, val) {
        var handle = typeof val === 'object' ? val ? val.handle : null : String(val);
        bindings.setObjectArrayItem(this.handle, index, handle);
    }
};

function initBinding() {
    var verbose = !!process.env.NODE_JAVA_VERBOSE;

    var java_home = process.env.JAVA_HOME,
        jre_home = process.env.JRE_HOME;
    if (java_home === undefined && jre_home === undefined) {
        throw new Error("neither JAVA_HOME nor JRE_HOME is found in environment variables");
    }
    var fs = require('fs');

    var platform = process.platform, arch = process.arch;

    switch (true) {
        case java_home && platform === "linux" && arch === "x64" && test(java_home + "/jre/lib/amd64/server/libjvm.so"):
        case jre_home && platform === "linux" && arch === "x64" && test(jre_home + "/lib/amd64/server/libjvm.so"):
            break;

        default:
            throw new Error("jvm shared library not found.");
    }


    function test(path) {
        if (verbose) {
            console.log('[INIT] testing jvm lib: ' + path);
        }
        if (fs.existsSync(path)) {
            bindings.link(path, verbose);
            return true;
        }
        if (verbose) {
            console.log('[INIT] file not found: ' + path);
        }
    }

}

