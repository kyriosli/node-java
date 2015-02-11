var java = require('../');

var vm = java.createVm();

var JavaRobot = vm.findClass('java/awt/Robot');

var robot = JavaRobot.newInstance();
