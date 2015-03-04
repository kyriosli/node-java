var assert = require('assert');
var java = require('../');

var vm = java.createVm();

var cls = vm.findClass("java/lang/String");

var obj = cls.asObject();

console.log("String.class.getName(): " + obj.invoke("getName()Ljava/lang/String;"));

var cls2 = vm.findClass("java/util/Date").newInstance().invoke("getClass()Ljava/lang/Class;");

console.log("new Date().getClass().getName(): " + cls2.invoke("getName()Ljava/lang/String;"));

console.log("new Date().getClass().newInstance(Date.now()).toString(): " + 
	cls2.asClass().newInstance("J", Date.now()).invoke("toString()Ljava/lang/String;"));
