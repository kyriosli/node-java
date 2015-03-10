#include "java.h"

using namespace v8;

void java::JavaObject::WeakCallback(const WeakCallbackData<External, JNIEnv>& data) {
	JNIEnv* env = data.GetParameter();
	JavaObject* ptr = (JavaObject*) data.GetValue()->Value();

	ptr->_ref.Reset();	
	env->DeleteGlobalRef(ptr->_obj);
	delete ptr;
}


void java::invoke(jvalue& ret, const bool isStatic, const char retType, JNIEnv* env, jobject obj, jmethodID methodID, jvalue* values) {
	if(isStatic) { // is static
		jclass cls = (jclass) obj;
		switch(retType) {
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
		switch(retType) {
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

Local<Value> java::convert(const char type, Isolate* isolate, JNIEnv* env, jvalue val) {

	switch(type) {
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
		return JavaObject::wrap(env, val.l, isolate);
	}
	return Local<Value>();
}
