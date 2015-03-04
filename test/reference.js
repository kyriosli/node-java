var java = require('../'), vm = java.createVm();
var count = 0;
function press() {

	for(var i=0; i<1000; i++) {
		vm.findClass('java/lang/String');
	}
	count += i;
	if(count % 1e5 == 0)
		console.log(count / 1e5 + '%');
	if(count < 1e7)
		setTimeout(press);
}

press();
