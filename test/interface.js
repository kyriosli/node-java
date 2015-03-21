/**
 * This file shows how to implement a interface with javascript function
 */
var assert = require('assert');

var java = require('../');

var vm = java.createVm();

var runCalled = null;

var RunnableImpl = vm.implement(['java/lang/Runnable'], {
    '<init>(I)V': function (i) {
        assert(i === 123);
        this.i = i;
    },
    'run()V': function () {
        assert(arguments.length === 0);
        runCalled = this
    }
});

var runnable = RunnableImpl.newInstance();

runnable.invoke('run()V');

assert.strictEqual(runCalled, runnable);