var java = require('../');

var vm = java.createVm();

var JavaRobot = vm.findClass('java/awt/Robot');

var robot = JavaRobot.newInstance();

robot.invoke('keyPress(I)V', 0x41);
robot.invoke('keyRelease(I)V', 0x41);

