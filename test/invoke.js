var assert = require('assert');

var java = require('../');

var vm = java.createVm();
var cls = vm.findClass("test/Test");

assert.strictEqual(vm.findClass("test/Test"), cls);

// newInstance
var javaObject = cls.newInstance('ZBCSIFDJLjava/lang/String;',
    true, 127, 'A', 4095, 1048575, 12.34, Math.PI, Date.now(), 'Hello world');

console.log('invoke hashCode() 10w times');

javaObject.invoke('hashCode()I'); // generate method cache

console.time('invoke10w');

for (var i = 0; i < 1e5; i++) {
    javaObject.invoke('hashCode()I');
}

console.timeEnd('invoke10w');

console.time('invoke');

javaObject.invoke('method(Ljava/lang/String;)V', 'test');

assert(javaObject.invoke('method(Z)Z', false));
assert.strictEqual(javaObject.invoke('method(B)B', 1), 2);
assert.strictEqual(javaObject.invoke('method(S)S', 1), 2);
assert.strictEqual(javaObject.invoke('method(C)C', '你好'), 20321);
assert.strictEqual(javaObject.invoke('method(I)I', 1234), ~1234);
assert.strictEqual(javaObject.invoke('method(I)I', -1234), ~-1234);
assert.strictEqual(javaObject.invoke('method(F)F', 12.5), 13.5);
assert.strictEqual(javaObject.invoke('method(D)D', 12.5), 12.5 + Math.PI);
assert.strictEqual(javaObject.invoke('method(J)J', 1234), 1234 + 10000000000000000);
assert.strictEqual(javaObject.invoke('method(CC)Ljava/lang/String;', 0x41, 0x42), "AB");
var dummy = javaObject.invoke("method(Ltest/Test;)Ltest/Test;", javaObject);
assert(javaObject.invoke("equals(Ljava/lang/Object;)Z", dummy));

assert.strictEqual(javaObject.invoke('nullMethod()Ljava/lang/Object;'), null);
assert.strictEqual(javaObject.invoke('nullStringMethod()Ljava/lang/String;'), null);
assert.strictEqual(javaObject.invoke("method(Ltest/Test;)Ltest/Test;", null), null);

var str = javaObject.invoke("method(Ljava/lang/String;I)Ljava/lang/Object;", "foobar", 3);
assert.strictEqual(javaObject.invoke("method(Ljava/lang/String;C)Ljava/lang/String;", str, 'z'), 'barz');

var _e;
try {
    javaObject.invoke('throwRuntimeException(Ljava/lang/String;)V', 'sync')

} catch (e) {
    assert(/^java.lang.RuntimeException : \d{13}: sync$/.test(e.message));
    _e = true;
}
assert(_e);

console.timeEnd('invoke');

assert.strictEqual(javaObject.getClass(), cls);
assert.strictEqual(dummy.getClass(), cls);

_e = null;
try {
    javaObject.invoke('notFound()V')

} catch (e) {
    assert.strictEqual(e.message, "method `notFound' with signature `()V' not found.");
    _e = true;
}
assert(_e);


// new Date().toString()
console.log("new Date(Date.now()).toString(): " + vm.findClass("java/util/Date").newInstance("J", Date.now()).invoke("toString()Ljava/lang/String;"));
console.time('static');
// invokeStatic
var System = vm.findClass('java/lang/System');
console.log("System.currentTimeMillis(): " + System.invoke('currentTimeMillis()J'));
console.log("Math.random(): " + vm.findClass("java/lang/Math").invoke('random()D'));

console.timeEnd('static');

var _async_ok = false;
// invoke async
javaObject.invokeAsync("method(Ltest/Test;)Ltest/Test;", javaObject).then(function (dummy) {
    assert(dummy);
    return dummy.invokeAsync('throwRuntimeException(Ljava/lang/String;)V', 'async');
}, function (err) {
    throw err
}).then(function () {
    throw new Error('should not be here')
}, function (err) {
    assert(/^java.lang.RuntimeException : \d{13}: async$/.test(e.message));
    _async_ok = true;
});

process.on('exit', function () {
    if (!_async_ok) {
        console.log('async_ok not set');
    }
});