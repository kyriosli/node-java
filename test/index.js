var assert = require('assert');

var java = require('../');

var vm = java.createVm();

// Test findClass
var  javaClass = vm.findClass('test/Test');

// newInstance
var javaObject = javaClass.newInstance('ZBCSIFDJLjava/lang/String;', 
	true, 127, 'A', 4095, 1048575, 12.34, Math.PI, Date.now(), 'Hello world');

javaObject.invoke('method(Ljava/lang/String;)V', 'test');

assert(javaObject.invoke('method(Z)Z', false));
assert.strictEqual(javaObject.invoke('method(B)B', 1), 2);
assert.strictEqual(javaObject.invoke('method(S)S', 1), 2);
assert.strictEqual(javaObject.invoke('method(C)C', 1), 2);
assert.strictEqual(javaObject.invoke('method(I)I', 1234), ~1234);
assert.strictEqual(javaObject.invoke('method(F)F', 12.5), 13.5);
assert.strictEqual(javaObject.invoke('method(D)D', 12.5), 12.5 + Math.PI);
assert.strictEqual(javaObject.invoke('method(J)J', 1234), 1234 + 10000000000000000);
assert.strictEqual(javaObject.invoke('method(CC)Ljava/lang/String;', 0x41, 0x42), "AB");
var dummy = javaObject.invoke("method(Ltest/Test;)Ltest/Test;", javaObject);
assert(javaObject.invoke("equals(Ljava/lang/Object;)Z", dummy));

var str = javaObject.invoke("method(Ljava/lang/String;I)Ljava/lang/Object;", "foobar", 3);
assert.strictEqual(javaObject.invoke("method(Ljava/lang/String;C)Ljava/lang/String;", str, 'z'), 'barz');

var err;
try {
	vm.runMain("test/Test1", []);
} catch(e) {
	err = e;
}
assert(err);
assert.strictEqual(err.message, "java.lang.NoClassDefFoundError : test/Test1");

vm.runMain("test/Test", [1, 2, 3]);
console.log("vm.runMain() has returned");

vm = null;

setTimeout(function(){
	var vm = java.createVm();
	console.log('another vm instance created');

	vm.runMain("test/Test", [1, 2, 3]);
	vm.destroy();
	console.log("vm destroyed");
}, 1000);



