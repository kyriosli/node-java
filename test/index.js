var assert = require('assert');

var java = require('../');

var vm = java.createVm();

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



