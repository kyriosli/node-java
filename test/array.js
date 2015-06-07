var assert = require('assert');
var java = require('../');

var vm = java.createVm();
var str = vm.findClass('java/lang/String').newInstance('Ljava/lang/String;', 'aa bb cc');
var arr = str.invoke('split(Ljava/lang/String;)[Ljava/lang/String;', ' ').asObjectArray();

assert.strictEqual(arr.length, 3);

var aa = arr.get(0).invoke('toString()Ljava/lang/String;');
assert.strictEqual(aa, 'aa');
var bb = arr.get(1).invoke('toString()Ljava/lang/String;');
assert.strictEqual(bb, 'bb');
var cc = arr.get(2).invoke('toString()Ljava/lang/String;');
assert.strictEqual(cc, 'cc');

var _e = true;
try {
    arr.get(3);
    _e = false;
} catch (e) {
    assert.strictEqual(e.message, 'array index out of range');
    _e = true;
}
assert(_e);


// test set
arr.set(2, 'ddd');
cc = arr.get(2).invoke('toString()Ljava/lang/String;');
assert.strictEqual(cc, 'ddd');
// test set null
arr.set(2, null);
cc = arr.get(2);
assert.strictEqual(cc, null);
// test set different type

try {
    arr.set(1, arr);
    _e = false;
} catch (e) {
    assert.strictEqual(e.message, 'java.lang.ArrayStoreException');
    _e = true;
}
assert(_e);


try {
    arr.set(3, 'ddd');
    _e = false;
} catch (e) {
    assert.strictEqual(e.message, 'array index out of range');
    _e = true;
}
assert(_e);

// byte array
arr = str.invoke('getBytes()[B').asByteArray();

assert.strictEqual(arr.length, 8);
var buf = arr.getRegion();
assert.strictEqual(buf.length, 8);
assert.strictEqual(buf.toString(), 'aa bb cc');

// char array
arr = str.invoke('toCharArray()[C').asCharArray();
assert.strictEqual(arr.length, 8);
buf = arr.getRegion();
assert.strictEqual(buf.length, 16);
assert.strictEqual(buf.toString('utf16le'), 'aa bb cc');

buf[2] = 'd'.charCodeAt(0);
console.log(buf);
arr.setRegion(buf);
assert.strictEqual(vm.findClass('java/lang/String').invoke('valueOf([C)Ljava/lang/String;', arr), 'ad bb cc');