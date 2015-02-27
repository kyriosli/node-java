var java = require('../'), vm = java.createVm();

for(var i=0; i<1000; i++) {
	vm.findClass('java/lang/String');
}
console.log(i + ' references created');

setTimeout(function(){}, 1e6);
