#ifndef    node_java_h_
#define    node_java_h_

#include<jni.h>
#include<node.h>


#define JNI_VERSION    JNI_VERSION_1_6
#define THROW(msg)    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, msg)))
#define GET_PTR(args, idx, type)    static_cast<type>(External::Cast(*args[idx])->Value())
#define CHECK_ERRNO(errno, msg)    if(errno) {\
        char errMsg[128];\
        sprintf(errMsg, msg " with error code: %d", errno);\
        THROW(errMsg);\
        return;\
    }

#define GET_ENV(jvm, env) jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION)
#define SAFE_GET_ENV(jvm, env) CHECK_ERRNO(GET_ENV(jvm, env), "`GetEnv' failed")


#define RETURN(val)    args.GetReturnValue().Set(val)
#define UNWRAP(arg, type)    static_cast<type>(static_cast<JavaObject*>(External::Cast(*arg)->Value())->_obj)

namespace java {

    using namespace v8;

    /*
    * reads and clears java exception info
    */
    const jchar *getJavaException(JNIEnv *env, int &len);

    class JavaObject {
    private:
        Persistent <External> _ref;

        static void WeakCallback(const WeakCallbackData <External, JavaObject> &data);

        inline JavaObject(JavaVM *jvm, JNIEnv *env, Isolate *isolate, jobject obj) :
                _ref(isolate, External::New(isolate, this)),
                jvm(jvm), env(env), _obj(obj) {
            _ref.SetWeak(this, WeakCallback);
            _ref.MarkIndependent();
        }

    public:
        JavaVM *jvm;
        JNIEnv *env;
        jobject _obj;

        static inline Local <Value> wrap(JavaVM *jvm, JNIEnv *env, jobject obj, Isolate *isolate) {
            if (!obj) return Local<Value>();
            JavaObject *ptr = new JavaObject(jvm, env, isolate, env->NewGlobalRef(obj));
            env->DeleteLocalRef(obj);
            return Local<External>::New(isolate, ptr->_ref);
        }
    };

    class JavaMethod {
    private:
        char _argTypes[16];
    public:
        jmethodID methodID;
        bool isStatic;
        const char *argTypes;
        int args;
        char retType;

        inline JavaMethod(jmethodID methodID, bool isStatic) :
                isStatic(isStatic), methodID(methodID), argTypes(_argTypes) {
        }

        inline Local <External> wrap(Isolate *isolate) {
            return External::New(isolate, this);
        }
    };

    inline Local <String> cast(JNIEnv *env, Isolate *isolate, jstring javastr) {
        if (!javastr) {
            return Local<String>();
        }

        jsize strlen = env->GetStringLength(javastr);
        const jchar *chars = env->GetStringCritical(javastr, NULL);
        Local <String> ret = String::NewFromTwoByte(isolate, chars, String::kNormalString, strlen);
        env->ReleaseStringCritical(javastr, chars);
        return ret;
    }

    inline jstring cast(JNIEnv * env, Local < Value > jsval) {
        if (jsval->IsNull()) return NULL;
        String::Value val(jsval);
        int len = val.length();

        return env->NewString(*val, len);
    }

    void invoke(JNIEnv *env, jobject obj, JavaMethod *method, jvalue *values, jvalue &ret);

    Local <Value> convert(const char type, Isolate *isolate, JavaVM *jvm, JNIEnv *env, jvalue val);

    inline void ThrowException(const jchar *msg, int msgLen, Isolate *isolate) {
        Local <String> jsmsg = String::NewFromTwoByte(isolate, msg, String::kNormalString, msgLen);
        isolate->ThrowException(Exception::Error(jsmsg));
        delete[] msg;
    }

    inline void ThrowJavaException(JNIEnv * env, Isolate * isolate) {
        int len;
        const jchar *msg = getJavaException(env, &len);
        ThrowException(msg, len, isolate);
    }

    namespace vm {

        inline jobject unwrap(Local < Value > val) {
            Local <Object> obj = val->ToObject();
            Local <Value> handle = obj->Get(String::NewFromUtf8(Isolate::GetCurrent(), "handle"));
            if (!handle->IsExternal()) {
                return NULL;
            }
            return UNWRAP(handle, jobject);

        }

        inline void parseValue(jvalue &val, const char type, Local <Value> arg, JNIEnv *env) {
            switch (type) {
                case '$': // convert to jstring
                    val.l = arg->IsNull() ? NULL : arg->IsObject() ? unwrap(arg) : cast(env, arg);
                    break;
                case 'L':
                    val.l = arg->IsNull() ? NULL : unwrap(arg);
                    break;
                case 'Z':
                    val.z = arg->BooleanValue();
                    break;
                case 'B':
                    val.b = arg->Uint32Value();
                    break;
                case 'C':
                    val.c = arg->IsInt32() ? arg->Uint32Value() : **String::Value(arg);
                    break;
                case 'S':
                    val.s = arg->Int32Value();
                    break;
                case 'I':
                    val.i = arg->Int32Value();
                    break;
                case 'F':
                    val.f = arg->NumberValue();
                    break;
                case 'D':
                    val.d = arg->NumberValue();
                    break;
                case 'J':
                    val.j = arg->IntegerValue();
                    break;
            }
        }

        inline void parseValues(JNIEnv *env, JavaMethod *method, Local <Value> args, jvalue *values) {
            int count = method->args;
            const char *types = method->argTypes;

            Local <Object> $args = Local<Object>::Cast(args);
            for (int i = 0; i < count; i++) {
                parseValue(values[i], types[i], $args->Get(i + 1), env);
            }
        }

    }

}


#endif
