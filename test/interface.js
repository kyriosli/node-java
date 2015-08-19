/**
 * This file shows how to implement a interface with javascript function
 */
var assert = require('assert');

var java = require('../');

var vm = java.createVm();

var runCalled = false;

var RunnableImpl = vm.implement(['java/lang/Runnable'], {
    'run()V': function () {
        console.log('run()V called');
        assert(arguments.length === 0);
        runCalled = true
    }
});

var runnable = RunnableImpl.newInstance();

runnable.invoke('run()V');

assert(runCalled);

var TestImpl = vm.implement({
    'voidMethod(ZBCI)V': function (z, b, c, i) {
        console.log('voidMethod', z, b, c, i);
        assert(z);
        assert.strictEqual(b, 12);
        assert.strictEqual(c, 97);
        assert.strictEqual(i, 2015);
    }, 'stringMethod(ILjava/lang/String;D)Ljava/lang/String;': function (i, s, d) {
        assert.strictEqual(i, 34);
        assert.strictEqual(s, "Hello world");
        assert.strictEqual(d, Math.PI);
        return i + s + d;
    },
    'longMethod(J)J': function (i) {
        assert.strictEqual(i, 1e16);
        return Date.now();
    },
    'throws()V': function () {
        throw new Error("foo bar");
    },
    'test()V': function () {
    },
    'stringMethod(Ljava/lang/String;)Ljava/lang/String;': function (s) {
        return s + ':' + typeof s;
    }
});

var test = TestImpl.newInstance();

test.invoke('voidMethod(ZBCI)V', true, 12, 'a', 2015);
var _e = false;
try {
    test.invoke('throws()V');
} catch (e) {
    assert.strictEqual(e.message, 'java.lang.RuntimeException: Uncaught Error: foo bar');
    _e = true;
}
assert(_e);

assert.strictEqual(
    test.invoke('stringMethod(ILjava/lang/String;D)Ljava/lang/String;', 34, 'Hello world', Math.PI),
    '34Hello world' + Math.PI
);

assert.strictEqual(
    test.invoke('stringMethod(Ljava/lang/String;)Ljava/lang/String;', null),
    'null:object'
);


var diff = Date.now() - test.invoke('longMethod(J)J', 1e16);
assert(diff >= -1 && diff <= 0);


// extends super class
var written = [];
var myWriter = vm.findClass('java/io/PrintStream').newInstance('Ljava/io/OutputStream;', vm.implement('java/io/OutputStream', {
    'write(I)V': function (ch) {
        written.push(ch);
        process.stdout.write(new Buffer([ch]));
    }
}).newInstance());

myWriter.invoke('print(Ljava/lang/String;)V', 'Hello world!');

assert.strictEqual(new Buffer(written).toString(), 'Hello world!');

var threadStarted = false;

var timer = setTimeout(function () {
    assert(threadStarted);
}, 1000);

var Thread = vm.findClass('java/lang/Thread').newInstance('Ljava/lang/Runnable;', vm.implement(['java/lang/Runnable'], {
    'run()V': function () {
        assert(arguments.length === 0);
        threadStarted = true;
        clearTimeout(timer);
        console.log('thread started');
    }
}).newInstance());

Thread.invoke('start()V');
