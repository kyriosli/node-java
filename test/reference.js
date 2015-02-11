var java = require('../'), vm = java.createVm();

var n = 40;

var count = 0;

sched();

function sched() {
	for(var i=0; i<1000; i++) {
		vm.findClass('java/lang/String');
	}
	console.log((count+=1000) + ' references created');
	if(n--) setTimeout(sched, 500);
}
