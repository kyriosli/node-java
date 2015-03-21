/**
 * Created by kyrios.li on 2015/3/19.
 */

exports.Builder = Builder;

exports.constants = {
    ACC_PUBLIC: 1,
    ACC_PRIVATE: 2,
    ACC_PROTECTED: 4,
    ACC_STATIC: 8,
    ACC_FINAL: 0x10,
    ACC_SUPER: 0x20,
    ACC_SYNCHRONIZED: 0x20,
    ACC_BRIDGE: 0x40,
    ACC_VOLATILE: 0x40,
    ACC_VARARGS: 0x80,
    ACC_TRANSIENT: 0x80,
    ACC_NATIVE: 0x100,
    ACC_INTERFACE: 0x200,
    ACC_ABSTRACT: 0x400,
    ACC_STRICT: 0x800,
    ACC_SYNTHETIC: 0x1000,
    ACC_ANNOTATION: 0x2000,
    ACC_ENUM: 0x4000,
    REF_FIELD: 9,
    REF_METHOD: 10
};

function Builder(name, parentName, interfaceNames, accessFlags, attributes) {
    this.constants = [];

    this.accessFlags = accessFlags;
    this.classIdx = this.getClass(name);
    this.parentClassIdx = this.getClass(parentName);
    this.interfaceIdxes = interfaceNames.map(this.getClass.bind(this));

    this.fields = [];
    this.methods = [];
    this.attributes = attributes;
}

Builder.prototype = {
    addConstant: function (obj) {
        for (var i = 0, L = this.constants.length; i < L; i++) {
            var found = this.constants[i];
            if (found.tag === obj.tag && found.value === obj.value) {
                return i + 1;
            }
        }

        return this.constants.push(obj);
    },
    getUtf8: function (str) {
        return this.addConstant({
            tag: 1,
            size: 3 + str.replace(/[\x80-\u07ff]/g, '**').replace(/[\u0800-\uffff]/g, '***').length,
            value: str
        });
    },
    getString: function (str) {
        return this.addConstant({
            tag: 8,
            size: 3,
            index: this.getUtf8(str),
            value: str
        });
    },
    getInt: function (value) {
        return this.addConstant({
            tag: 3,
            size: 5,
            value: value
        });
    },
    getFloat: function (value) {
        return this.addConstant({
            tag: 4,
            size: 5,
            value: value
        });
    },
    getLong: function (value) {
        return this.addConstant({
            tag: 5,
            size: 5,
            value: value
        });
    },
    getDouble: function (value) {
        return this.addConstant({
            tag: 6,
            size: 5,
            value: value
        });
    },
    getClass: function (name) {
        return this.addConstant({
            tag: 7,
            size: 3,
            index: this.getUtf8(name),
            value: name
        });
    },
    getNameAndType: function (name, type) {
        var key = name + type;
        return this.addConstant({
            tag: 12,
            size: 5,
            index: this.getUtf8(name),
            index2: this.getUtf8(type),
            value: key
        });
    },
    getMemberRef: function (tag, className, name, type) {
        var key = className + '::' + name + type;
        return this.addConstant({
            tag: tag,
            size: 5,
            index: this.getClass(className),
            index2: this.getNameAndType(name, type),
            value: key
        });
    },
    defineMethod: function (name, signature, modifier, attributes) {
        //console.log('define method ' + name + signature);
        this.methods.push({
            modifier: modifier,
            nameIdx: this.getUtf8(name),
            typeIdx: this.getUtf8(signature),
            attributes: attributes
        })
    },
    defineField: function (name, type, modifier, attributes) {
        this.fields.push({
            modifier: modifier,
            nameIdx: this.getUtf8(name),
            typeIdx: this.getUtf8(type),
            attributes: attributes
        })
    },
    createCode: function (maxStack, maxLocals, instructions) {
        var codeLen = instructions.length,
            arr = new Uint8Array(12 + codeLen),
            view = new DataView(arr.buffer);
        view.setUint16(0, maxStack);
        view.setUint16(2, maxLocals);
        view.setUint32(4, codeLen);
        arr.set(instructions, 8);
        view.setUint32(8 + codeLen, 0);

        return {
            nameIdx: this.getUtf8('Code'),
            size: 18 + instructions.length, // TODO
            data: arr
        }
    }, build: function () {
        var self = this;
        var bufLen = 10; // magic code, versions, const_pool_count
        this.constants.forEach(addSize);
        bufLen += 8 + this.interfaceIdxes.length * 2;

        bufLen += 2 + this.fields.length * 8;
        this.fields.forEach(addAttributeSize);

        bufLen += 2 + this.methods.length * 8;
        this.methods.forEach(addAttributeSize);

        bufLen += 2; // attributes_count
        addAttributeSize(this);

        var buffer = new ArrayBuffer(bufLen),
            bytes = new Uint8Array(buffer),
            view = new DataView(buffer),
            idx;
        view.setUint32(0, 0xcafebabe);
        view.setUint32(4, 0x00000034);
        view.setUint16(8, this.constants.length + 1);

        idx = 10;

        this.constants.forEach(function (constant) {
            var val = constant.value;
            bytes[idx++] = constant.tag;
            if (constant.size === 3) {
                view.setUint16(idx, constant.index);
                idx += 2;
            } else if (constant.tag === 1) { // utf8
                view.setUint16(idx, constant.size - 3);
                idx += 2;
                for (var i = 0, l = val.length; i < l; i++) {
                    var ch = val.charCodeAt(i) & 0xffff;
                    if (ch < 0x80) { // single byte
                        bytes[idx++] = ch;
                    } else if (ch < 0x800) { // double bytes
                        bytes[idx++] = 0xc0 | ch >> 6;
                        bytes[idx++] = 0x80 | ch & 0x3f;
                    } else { // triple bytes
                        bytes[idx++] = 0xe0 | ch >> 12;
                        bytes[idx++] = 0x80 | ch >> 6 & 0x3f;
                        bytes[idx++] = 0x80 | ch & 0x3f;
                    }
                }
            } else if (constant.tag === 3) { // int
                view.setInt32(idx, val | 0);
                idx += 4;
            } else if (constant.tag === 4) { // float
                view.setFloat32(idx, val);
                idx += 4;
            } else if (constant.tag === 5) { // long
                view.setInt32(idx, val / 0x100000000 | 0);
                view.setInt32(idx + 4, val | 0);
                idx += 8;
            } else if (constant.tag === 6) { // double
                view.setFloat64(idx, val);
                idx += 8;
            } else { // two indexes
                view.setUint16(idx, constant.index);
                view.setUint16(idx + 2, constant.index2);
                idx += 4;
            }
        });
        view.setUint16(idx, this.accessFlags);
        view.setUint16(idx + 2, this.classIdx);
        view.setUint16(idx + 4, this.parentClassIdx);
        view.setUint16(idx + 6, this.interfaceIdxes.length);
        idx += 8;
        this.interfaceIdxes.forEach(function (interfaceIdx) {
            view.setUint16(idx, interfaceIdx);
            idx += 2;
        });
        view.setUint16(idx, this.fields.length);
        idx += 2;
        this.fields.forEach(handleMember);

        view.setUint16(idx, this.methods.length);
        idx += 2;
        this.methods.forEach(handleMember);

        handleAttributes(this.attributes);
        return buffer;

        function addAttributeSize(obj) {
            obj.attributes && obj.attributes.forEach(addSize);
        }

        function addSize(obj) {
            bufLen += obj.size | 0;
        }

        function handleMember(member) {
            view.setUint16(idx, member.modifier);
            view.setUint16(idx + 2, member.nameIdx);
            view.setUint16(idx + 4, member.typeIdx);
            idx += 6;
            handleAttributes(member.attributes);
        }

        function handleAttributes(attrs) {
            if (attrs) {
                view.setUint16(idx, attrs.length);
                idx += 2;
                attrs.forEach(handleAttribute);
            } else {
                view.setUint16(idx, 0);
                idx += 2;
            }
        }

        function handleAttribute(attribute) {
            //console.log('set name', attribute.nameIdx);
            view.setUint16(idx, attribute.nameIdx);
            //console.log('set size', idx + 2, bufLen);
            view.setUint32(idx + 2, attribute.size - 6);
            bytes.set(attribute.data, idx + 6);
            idx += attribute.size;
        }
    }
};