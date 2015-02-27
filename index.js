var bindings = require('./java.node');
// creates an java vm.
// We do not reinitialize a new vm if one has already been created and we losts its reference.
// And note that once a vm is destroyed, createVm becomes unstable, its behavior is vendor-dependent.
exports.createVm = function() {
	// TODO: options
	var options = []//["-verbose:jni"];
	// Because JavaVMOption accepts zero-terminated cstring as its input, we must
	// make sure that the input strings does not contain \0.

	return new JavaVM(bindings.createVm(options.length, options.join('\0')));
}

function JavaVM(vm) {
	this.vm = vm;
}

JavaVM.prototype = {
	findClass: function(name) {
		return new JavaClass(this.vm, bindings.findClass(this.vm, name));
	},
	runMain: function(cls, args) {
		// cls is of form com/pkg/name/ClassName$SubClassName
		bindings.runMain(this.vm, cls, args);
	},
	destroy: function() {
		bindings.dispose(this.vm);
		this.vm = null;
	}
};

function JavaClass(vm, handle) {
	this.vm = vm;
	this.handle = handle;

	this.methodCache = {};
}

var rType = /\[*(?:L[^;]+;|[ZBCSIFDJ])/g;

JavaClass.prototype = {
	newInstance: function(signature) {
		signature = signature || '';
		var method = this.findMethod('<init>(' + signature + ')V');
		var ref = bindings.newInstance(this.vm, this.handle, method.id, method.types, parseArgs(arguments, method.types));
		return new JavaObject(this.vm, this, ref);
	},
	findMethod: function(signature) {
		if(signature in this.methodCache) {
			return this.methodCache[signature];
		}
		var idx = signature.indexOf('('), 
		name = signature.substr(0, idx), type = signature.substr(idx);
		var methodID = bindings.findMethod(this.vm, this.handle, name, type);
		var idx2 = type.indexOf(')'), arg = type.substring(1, idx2);
		var argTypes = '', m;
		while(m = rType.exec(arg)) {
			var argType = m[0][0];
			argTypes += argType === '[' ? 'L' : 
				argType === 'L' && m[0] === 'Ljava/lang/String;' ? '$' :
				argType;
		}

		var retType = type[idx2 + 1];

		return this.methodCache[signature] = {
			id: methodID,
			types: argTypes,
			retType: retType === '[' ? 'L' : 
				retType === 'L' && type.substr(idx2 + 1) === 'Ljava/lang/String;' ? '$' :
				retType
		};
	}
};

function parseArgs(args, types) {
	var l, ret = Array.prototype.slice.call(args, 1);
	for(var i = 0, l = types.length; i < l; i++) {
		var type = types[i];
		if(type === 'L') { // object
			var val = ret[i];
			ret[i] = val && val.handle || null;
		} else if(type === '$') { // string
			// a java string maybe passed as a JavaObject
			val = ret[i];
			if(val) {
				ret[i] = val.handle || String(val);
			} else if(val === undefined) {
				ret[i] = null;
			}
		}
	}
	return ret;
}

function JavaObject(vm, cls, handle) {
	this.vm = vm;
	this.cls = cls;
	this.handle = handle;
}

JavaObject.prototype = {
	getClass: function() {
		var cls = this.cls;
		if(!cls) {
			// TODO
		}
		return cls;
	},
	invoke: function(signature) {
		var method = this.getClass().findMethod(signature);
		console.log(method);
		var ret = bindings.invoke(this.vm, this.handle, method.id, method.types, parseArgs(arguments, method.types), method.retType);
		if(method.retType === 'L') ret = new JavaObject(this.vm, null, ret);
		return ret;
	}
}

