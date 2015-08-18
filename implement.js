/**
 * Build a .class file that implements several interfaces with javascript code
 */
var cb = require('./classBuilder'),
    Builder = cb.Builder,
    constants = cb.constants;

var classFlags = constants.ACC_PUBLIC | constants.ACC_FINAL | constants.ACC_SYNTHETIC,
    helperFlags = constants.ACC_PRIVATE | constants.ACC_STATIC | constants.ACC_NATIVE;

var rSignature = /^(\w+|<init>)(\(((?:\[*(?:L\w+(?:\/\w+)*;|.))*)\)(\[*(?:L\w+(?:\/\w+)*;|.)))$/;
var $SIPUSH = 0x11, $LDC = 0x12, $LDC_W = 0x13, $GETFIELD = 0xb4, $WIDE = 0xc4, $INVOKESTATIC = 0xb8, $ALOAD_0 = 0x2a;

var fullBuf = new Uint8Array(4096), codeBuf = fullBuf.subarray(4);

fullBuf[0] = $ALOAD_0;
fullBuf[1] = 0xb7;
var maxStack;

exports.build = function (name, superName, interfaces, methods) {
    var builder = new Builder(name, superName, interfaces, classFlags);

    var superCtor = builder.getMemberRef(constants.REF_METHOD, superName, '<init>', '()V');

    fullBuf[2] = superCtor >> 8;
    fullBuf[3] = superCtor;

    var nameIdx = builder.getString(name);
    var callHelpers = [], instructions = {};

    var hasCtor = false;

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

        var callType = '(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;' + argTypes.replace(/L/g, 'Ljava/lang/Object;') + ')' +
            (retType === 'L' ? 'Ljava/lang/Object;' : retType);

        // console.log(signature, argTypes, retType, callType);


        if (!(callType in instructions)) {
            callHelpers.push(callType);
            instructions[callType] = true;
        }

        var callerIdx = builder.getMemberRef(constants.REF_METHOD, name, '_$CALLJS', callType);
        var castRet = retType === 'L' && fullRetType !== 'Ljava/lang/Object;' ? builder.getClass(fullRetType) : 0;

        var methodNameIdx = builder.getString(methodName),
            methodTypeIdx = builder.getString(methodTypes);

        var isCtor = methodName === '<init>';

        if (isCtor) {
            hasCtor = true;
        }
        var code = buildInstructions(nameIdx, methodNameIdx, methodTypeIdx, argTypes, retType, castRet, callerIdx, isCtor);
        builder.defineMethod('_$CALLJS', callType, helperFlags);

        // console.log(methodName, methodTypes, code);
        console.log('defineMethod[' + signature + '] maxStack=' + maxStack, isCtor);
        builder.defineMethod(methodName, methodTypes, constants.ACC_PUBLIC, [builder.createCode(maxStack, maxStack - 2, code)]);
    }

    if (!hasCtor) {
        // add default constructor if none supplied
        codeBuf[0] = 0xb1; // return
        //console.log('defineMethod[<init>()V] maxStack=1', true, fullBuf.subarray(0, 5));
        builder.defineMethod('<init>', '()V', constants.ACC_PUBLIC, [builder.createCode(1, 1, fullBuf.subarray(0, 5))]);
    }

    return {buffer: builder.build(), natives: callHelpers};

};


function buildInstructions(nameIdx, methodNameIdx, methodTypeIdx, argTypes, retType, retClass, callerIdx, isCtor) {
    var buf = codeBuf, idx = 12;

    buf[0] = $LDC;
    buf[1] = nameIdx;
    buf[2] = $LDC_W;
    // 3 and 4 is index of method name
    buf[3] = methodNameIdx >> 8;
    buf[4] = methodNameIdx;
    buf[5] = $LDC_W;
    // 6 and 7 is index of method type
    buf[6] = methodTypeIdx >> 8;
    buf[7] = methodTypeIdx;
    maxStack = 3;

    for (var i = 0, L = argTypes.length; i < L;) {
        var type = argTypes[i++];
        maxStack += type == 'J' || type == 'D' ? 2 : 1;
        if (i < 4) {
            buf[idx++] = (
                type === 'L' ? $ALOAD_0//aload_i
                    : type === 'F' ? 0x22//fload_i
                    : type === 'J' ? 0x1e//lload_i
                    : type === 'D' ? 0x26//dload_i
                    : 0x1a//iload_i
            ) + i;
        } else if (i < 256) {
            buf[idx++] =
                type === 'L' ? 0x19//aload
                    : type === 'F' ? 0x17//fload_i
                    : type === 'J' ? 0x16//lload_i
                    : type === 'D' ? 0x18//dload_i
                    : 0x15;//iload_i
            buf[idx++] = i;
        } else {
            buf[idx++] = $WIDE;
            buf[idx++] =
                type === 'L' ? 0x19//aload
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

    return isCtor ? fullBuf.subarray(0, idx + 4) : buf.subarray(0, idx);
}
