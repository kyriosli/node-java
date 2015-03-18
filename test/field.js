var assert = require('assert');
var java = require('../');

var vm = java.createVm();

var System = java.findClass('java/lang/System');

var out = System.get('out', 'Ljava/io/PrintStream;');

out.invoke('println(Ljava/lang/String;)V', 'Hello world');