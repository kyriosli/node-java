/**
 * Build a .class file that implements several interfaces with javascript code
 */
var cb = require('./classBuilder'),
    Builder = cb.Builder,
    constants = cb.constants;

var classFlags = constants.ACC_PUBLIC | constants.ACC_FINAL | constants.ACC_SYNTHETIC,
    helperFlags = constants.ACC_PRIVATE | constants.ACC_STATIC | constants.ACC_NATIVE;

var rSignature = /^(\w+)(\(((?:\[*(?:L\w+(?:\/\w+)*;|.))*)\)(\[*(?:L\w+(?:\/\w+)*;|.)))$/;

exports.build = function (name, interfaces, methods) {
    var builder = new Builder(name, 'java/lang/Object', interfaces, classFlags);
    builder.defineField('J', 'J', constants.ACC_PRIVATE); // pointer to the c++ handle
    var pointerIdx = builder.getMemberRef(constants.REF_FIELD, name, 'J', 'J');

    var superInit = builder.getMemberRef(constants.REF_METHOD, 'java/lang/Object', '<init>', '()V');
    // add default constructor if none supplied
    builder.defineMethod('<init>', '(J)V', constants.ACC_PUBLIC, [builder.createCode(3, 3, new Uint8Array([
        $ALOAD_0,
        0xb7, superInit >> 8, superInit, // invokespecial Object.<init>
        $ALOAD_0,
        0x1f, // lload_1
        0xb5, pointerIdx >> 8, pointerIdx, // putfield
        0xb1
    ]))]);

    var nameIdx = builder.getString(name);
    var callHelpers = [], instructions = {};

    for (var ii = 0, ll = methods.length; ii < ll; ii++) {
        var signature = methods[ii];

        var m = signature.match(rSignature);
        if (!m) continue;

        var methodName = m[1], methodTypes = m[2];
        //console.log(m);
        var argTypes = m[3].replace(/\[+(?:L[^;]+;|.)/g, 'L').replace(/L[^;]+;/g, 'L').replace(/[ZBSC]/g, 'I'),
            retType = m[4],
            fullRetType = retType;
        if (retType.length > 1) retType = 'L';

        var callType = '(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J' + argTypes.replace(/[L$]/g, 'Ljava/lang/Object;') + ')' +
            (retType === '$' || retType === 'L' ? 'Ljava/lang/Object;' : retType);

        // console.log(signature, argTypes, retType, callType);

        var code;
        if (callType in instructions) {
            code = instructions[callType];
        } else {
            callHelpers.push(callType);
            var callerIdx = builder.getMemberRef(constants.REF_METHOD, name, '_$CALLJS', callType);

            code = callInstructions(nameIdx, pointerIdx, argTypes, retType, retType === 'L' && fullRetType !== 'Ljava/lang/Object;' ? builder.getClass(fullRetType) : 0, callerIdx);
            instructions[callType] = code;
            builder.defineMethod('_$CALLJS', callType, helperFlags);
        }
        var methodNameIdx = builder.getString(methodName),
            methodTypeIdx = builder.getString(methodTypes);
        code[3] = methodNameIdx >> 8;
        code[4] = methodNameIdx;
        code[6] = methodTypeIdx >> 8;
        code[7] = methodTypeIdx;
        //console.log(signatureIdx, callType, Array.prototype.slice.call(code));
        builder.defineMethod(methodName, methodTypes, constants.ACC_PUBLIC, [builder.createCode(argTypes.length + 5, argTypes.length + 1, code)]);
    }


    return {buffer: builder.build(), natives: callHelpers};

};
var $SIPUSH = 0x11, $LDC = 0x12, $LDC_W = 0x13, $GETFIELD = 0xb4, $WIDE = 0xc4, $INVOKESTATIC = 0xb8, $ALOAD_0 = 0x2a;

var codeBuf = new Uint8Array(4096);

function callInstructions(nameIdx, pointerIdx, argTypes, retType, retClass, callerIdx) {
    var buf = codeBuf, idx = 12;

    buf[0] = $LDC;
    buf[1] = nameIdx;
    buf[2] = $LDC_W;
    // 3 and 4 is index of method name
    buf[5] = $LDC_W;
    // 6 and 7 is index of method type
    buf[8] = $ALOAD_0; //aload_0;
    buf[9] = $GETFIELD;
    buf[10] = pointerIdx >> 8;
    buf[11] = pointerIdx;

    for (var i = 0, L = argTypes.length; i < L;) {
        var type = argTypes[i++];
        if (i < 4) {
            buf[idx++] = (
                type === 'L' || type === '$' ? $ALOAD_0//aload_i
                    : type === 'F' ? 0x22//fload_i
                    : type === 'J' ? 0x1e//lload_i
                    : type === 'D' ? 0x26//dload_i
                    : 0x1a//iload_i
            ) + i;
        } else if (i < 256) {
            buf[idx++] =
                type === 'L' || type === '$' ? 0x19//aload
                    : type === 'F' ? 0x17//fload_i
                    : type === 'J' ? 0x16//lload_i
                    : type === 'D' ? 0x18//dload_i
                    : 0x15;//iload_i
            buf[idx++] = i;
        } else {
            buf[idx++] = $WIDE;
            buf[idx++] =
                type === 'L' || type === '$' ? 0x19//aload
                    : type === 'F' ? 0x17//fload_i
                    : type === 'J' ? 0x16//lload_i
                    : type === 'D' ? 0x18//dload_i
                    : 0x15;//iload_i
            buf[idx++] = i;
            buf[idx++] = i >> 8;
            buf[idx++] = i;
        }
    }
    buf[idx++] = $INVOKESTATIC;
    buf[idx++] = callerIdx >> 8;
    buf[idx++] = callerIdx;
    if (retType === 'L') {
        if (retClass > 0) {
            buf[idx++] = 0xc0; // checkcast
            buf[idx++] = retClass >> 8;
            buf[idx++] = retClass;

        }
        buf[idx++] = 0xb0; // areturn
    } else if (retType === 'F') {
        buf[idx++] = 0xae; // freturn
    } else if (retType === 'J') {
        buf[idx++] = 0xad; // lreturn
    } else if (retType === 'D') {
        buf[idx++] = 0xaf; // dreturn
    } else if (retType === 'V') {
        buf[idx++] = 0xb1; // return
    } else {
        buf[idx++] = 0xac; // ireturn
    }

    return buf.subarray(0, idx);
}