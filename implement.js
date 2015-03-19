/**
 * Build a .class file that implements several interfaces with javascript code
 */
var cb = require('./classBuilder'),
    Builder = cb.Builder,
    constants = cb.constants;

var classFlags = constants.ACC_PUBLIC | constants.ACC_FINAL | constants.ACC_SYNTHETIC,
    fieldFlags = constants.ACC_PUBLIC,
    methodFlags = constants.ACC_PUBLIC | constants.ACC_NATIVE;

exports.build = function (name, interfaces, methods) {
    var builder = new Builder(name, 'java/lang/Object', interfaces, classFlags);
    builder.defineField('$', 'J', fieldFlags);
    Object.keys(methods).forEach(function (signature) {
        var idx = signature.indexOf('(');
        builder.defineMethod(signature.substr(0, idx), signature.substr(idx), methodFlags);
    });
    return builder.build();

};
