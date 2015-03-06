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

var vm_destroyed = false;

setTimeout(function(){
	var vm = java.createVm();
	console.log('another vm instance created');

	vm.runMain("test/Test", [1, 2, 3]);

	vm.runMainAsync('test/Test', ["run", "main", "async"]).then(function() {
		return vm.runMainAsync('test/Test', ["willthrow"])
	}).then(function()	{
		throw new Error("should not be called")
	}, function(err) {
		console.error(err.stack);
		assert(err);
		vm.destroy();
		console.log("vm destroyed");
		vm_destroyed = true;
	});


}, 100);


process.on('exit', function() {
	if(!vm_destroyed) throw new Error("vm.destroy() not called");
});
