#ifndef    node_java_h_
#define    node_java_h_

#include<jni.h>
#include<node.h>


#define JNI_VERSION    JNI_VERSION_1_6
#define THROW(msg)    isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, msg)))
#define GET_PTR(args, idx, type)    static_cast<type>(External::Cast(*args[idx])->Value())
#define GET_JVM(args)    GET_PTR(args, 0, JavaVM*)
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
    const jchar *getJavaException(JNIEnv *env, int *len);

    class ExternalResource {

    private:
        Persistent <External> _ref;

        static void WeakCallback(const WeakCallbackData <External, ExternalResource> &data);

    protected:
        inline ExternalResource(Isolate *isolate) :
                _ref(isolate, External::New(isolate, this)) {
            _ref.SetWeak(this, WeakCallback);
            _ref.MarkIndependent();
        }

    public:
        virtual ~ExternalResource() = 0;

        inline Local <External> wrap(Isolate *isolate) {
            return Local<External>::New(isolate, _ref);
        }

    };

    class JavaObject : public ExternalResource {
    private:
        JNIEnv *env;

        inline JavaObject(JNIEnv *env, Isolate *isolate, jobject obj) :
                ExternalResource(isolate),
                env(env), _obj(env->NewGlobalRef(obj)) {
        }

    public:
        jobject _obj;

        static inline Local <Value> wrap(JNIEnv *env, jobject obj, Isolate *isolate) {
            if (!obj) return Local<Value>();
            ExternalResource *ptr = new JavaObject(env, isolate, obj);
            return ptr->wrap(isolate);
        }

        ~JavaObject();

    };

    class JavaMethod : public ExternalResource {
    private:
        char _argTypes[16];
    public:
        jmethodID methodID;
        bool isStatic;
        const char *argTypes;
        int args;
        char retType;

        inline JavaMethod(Isolate *isolate, jmethodID methodID, bool isStatic) :
                ExternalResource(isolate), isStatic(isStatic), methodID(methodID), argTypes(_argTypes) {
        }

        ~JavaMethod();

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

    void invoke(jvalue &ret, const bool isStatic, const char retType, JNIEnv *env, jobject obj, jmethodID methodID, jvalue *values);

    Local <Value> convert(const char type, Isolate *isolate, JNIEnv *env, jvalue val);

    const jchar *getJavaException(JNIEnv *env, int *len);

    inline void printw(jchar *out, const char *in, int len) {
        for (int i = 0; i < len; i++)
            out[i] = in[i];
    }

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

        inline void parseValue(jvalue &val, const char type, Local <Value> arg, JNIEnv *env) {
            switch (type) {
                case '$': // convert to jstring
                    val.l = arg->IsNull() ? NULL : arg->IsExternal() ? UNWRAP(arg, jstring) : cast(env, arg);
                    break;
                case 'L':
                    val.l = arg->IsNull() ? NULL : UNWRAP(arg, jobject);
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

        inline void parseValues(JNIEnv *env, JavaMethod *method, const FunctionCallbackInfo <Value> &args, jvalue *values, int start) {
            int count = method->args;
            const char *types = method->argTypes;

            for (int i = 0; i < count; i++) {
                parseValue(values[i], types[i], args[i + start], env);
            }
        }

        inline void parseValues(JNIEnv *env, jvalue *values, const char *types, Local <Array> args) {
            int count = args->Length();

            for (int i = 0; i < count; i++) {
                parseValue(values[i], types[i], args->Get(i), env);
            }

        }

    }

}


#endif
