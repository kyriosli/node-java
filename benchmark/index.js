var assert = require('assert');
var java = require('../');

var vm = java.createVm(),
    cls  = vm.findClass('test/Test');

(function () {
    var JavaMath = vm.findClass('java/lang/Math');
    assert.strictEqual(JavaMath.get('PI', 'D'), Math.PI);
    console.time('get java.lang.Math.PI (100w)');

    for (var i = 0; i < 1e6; i++) {
        JavaMath.get('PI', 'D');
    }

    console.timeEnd('get java.lang.Math.PI (100w)');
})();


(function () {
    var obj = cls.newInstance(),
        runnable = vm.implement(['java/lang/Runnable'], {
            'run()V': function () {
            }
        }).newInstance();
    console.time('invoke implemented method (10w)');
    obj.invoke('testRun(Ljava/lang/Runnable;I)V', runnable, 1e5);
    console.timeEnd('invoke implemented method (10w)');
})();


(function () {

    var javaObject = cls.newInstance();

    javaObject.invoke('hashCode()I'); // generate method cache

    console.time('invoke method with no args (100w)');

    for (var i = 0; i < 1e6; i++) {
        javaObject.invoke('hashCode()I');
    }

    console.timeEnd('invoke method with no args (100w)');

    javaObject.invoke('test(ZBCSIFDJ)V'); // generate method cache

    console.time('invoke method with 8 args (100w)');

    for (var i = 0; i < 1e6; i++) {
        javaObject.invoke('test(ZBCSIFDJ)V', 0, 0, 0, 0, 0, 0, 0, 0);
    }

    console.timeEnd('invoke method with 8 args (100w)');


})();

