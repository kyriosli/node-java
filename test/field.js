var assert = require('assert');
var java = require('../');

var vm = java.createVm();

var JavaSystem = vm.findClass('java/lang/System');

var out = JavaSystem.get('out', 'Ljava/io/PrintStream;');

out.invoke('println(Ljava/lang/String;)V', 'Hello world');

var JavaMath = vm.findClass('java/lang/Math');

assert.strictEqual(JavaMath.get('PI', 'D'), Math.PI);

var Test = vm.findClass('test/Test');

var javaObject = Test.newInstance('ZBCSIFDJLjava/lang/String;',
    true, 127, 'A', 4095, 1048575, 12.34, Math.PI, Date.now(), 'Hello world');

assert.strictEqual(javaObject.get('doubleField', 'D'), Math.PI);
assert.strictEqual(javaObject.get('stringField', 'Ljava/lang/String;'), 'Hello world');

javaObject.set('stringField', 'Ljava/lang/String;', 'Hello world2');
assert.strictEqual(javaObject.get('stringField', 'Ljava/lang/String;'), 'Hello world2');


var _e;
try {
    javaObject.get('not_exist', 'I');
} catch (e) {
    assert(/^field `not_exist' with type `I' not found.$/.test(e.message));
    _e = true;
}
assert(_e);

