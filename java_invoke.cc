#include "java.h"

using namespace v8;

void java::ExternalResource::WeakCallback(const WeakCallbackData <External, ExternalResource> &data) {
    ExternalResource *ptr = data.GetParameter();
    ptr->_ref.Reset();
    delete ptr;
}


java::JavaObject::~JavaObject() {
    env->DeleteGlobalRef(_obj);
}

java::JavaMethod::~JavaMethod() {
    if (args > 16) {
        delete[] argTypes;
    }
}

void java::invoke(JNIEnv *env, jobject obj, JavaMethod *method, jvalue *values, jvalue &ret) {
    jmethodID methodID = method->methodID;
    if (method->isStatic) { // is static
        jclass cls = (jclass) obj;
        switch (method->retType) {
            case 'V':
                env->CallStaticVoidMethodA(cls, methodID, values);
                break;
            case 'Z':
                ret.z = env->CallStaticBooleanMethodA(cls, methodID, values);
                break;
            case 'B':
                ret.b = env->CallStaticByteMethodA(cls, methodID, values);
                break;
            case 'S':
                ret.s = env->CallStaticShortMethodA(cls, methodID, values);
                break;
            case 'C':
                ret.c = env->CallStaticCharMethodA(cls, methodID, values);
                break;
            case 'I':
                ret.i = env->CallStaticIntMethodA(cls, methodID, values);
                break;
            case 'F':
                ret.f = env->CallStaticFloatMethodA(cls, methodID, values);
                break;
            case 'D':
                ret.d = env->CallStaticDoubleMethodA(cls, methodID, values);
                break;
            case 'J':
                ret.j = env->CallStaticLongMethodA(cls, methodID, values);
                break;
            case '$':
            case 'L':
                ret.l = env->CallStaticObjectMethodA(cls, methodID, values);
                break;
        }
    } else {
        switch (method->retType) {
            case 'V':
                env->CallVoidMethodA(obj, methodID, values);
                break;
            case 'Z':
                ret.z = env->CallBooleanMethodA(obj, methodID, values);
                break;
            case 'B':
                ret.b = env->CallByteMethodA(obj, methodID, values);
                break;
            case 'S':
                ret.s = env->CallShortMethodA(obj, methodID, values);
                break;
            case 'C':
                ret.c = env->CallCharMethodA(obj, methodID, values);
                break;
            case 'I':
                ret.i = env->CallIntMethodA(obj, methodID, values);
                break;
            case 'F':
                ret.f = env->CallFloatMethodA(obj, methodID, values);
                break;
            case 'D':
                ret.d = env->CallDoubleMethodA(obj, methodID, values);
                break;
            case 'J':
                ret.j = env->CallLongMethodA(obj, methodID, values);
                break;
            case '$':
            case 'L':
                ret.l = env->CallObjectMethodA(obj, methodID, values);
                break;
        }
    }
}

Local <Value> java::convert(const char type, Isolate *isolate, JavaVM *jvm, JNIEnv *env, jvalue val) {

    switch (type) {
        case 'Z':
            return Boolean::New(isolate, val.z);
        case 'B':
            return Integer::New(isolate, val.b);
        case 'S':
            return Integer::New(isolate, val.s);
        case 'C':
            return Integer::NewFromUnsigned(isolate, val.c);
        case 'I':
            return Integer::New(isolate, val.i);
        case 'F':
            return Number::New(isolate, val.f);
        case 'D':
            return Number::New(isolate, val.d);
        case 'J':
            return Number::New(isolate, val.j);
        case '$':
            return cast(env, isolate, (jstring) val.l);
        case 'L':
            return JavaObject::wrap(jvm, env, val.l, isolate);
    }
    return Local<Value>();
}
