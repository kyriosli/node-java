#ifndef    node_java_h_
#define    node_java_h_

#include<jni.h>
#include<node.h>
#include<nan.h>


#define JNI_VERSION    JNI_VERSION_1_6
#define GET_PTR(info, idx, type)    static_cast<type>(External::Cast(*info[idx])->Value())
#define CHECK_ERRNO(errno, msg)    if(errno) {\
        char errMsg[128];\
        sprintf(errMsg, msg " with error code: %d", errno);\
        return Nan::ThrowError(errMsg);\
    }

#define GET_ENV(jvm, env) jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION)
#define SAFE_GET_ENV(jvm, env) CHECK_ERRNO(GET_ENV(jvm, env), "`GetEnv' failed")

namespace java {

    extern bool verbose;

    typedef struct GlobalRefs {
        int count;
        jobject *globals;
        jobject _globals[16];

        inline GlobalRefs() : count(0), globals(_globals) {
        };


        inline void push(jobject obj) {
            if (count == 16) {
                globals = new jobject[256];
                // fprintf(stderr, "dynamic-allocating array %x\n", globals);
                for (int i = 0; i < 16; i++) globals[i] = _globals[i];
            }
            globals[count++] = obj;
        }

        template<typename S>
        inline void push(JNIEnv *env, S &obj) {
            push(obj = (S) env->NewGlobalRef(obj));
        }

        inline void free(JNIEnv *env) {
            for (int i = 0; i < count; i++) env->DeleteGlobalRef(globals[i]);
            if (count > 16) {
                // fprintf(stderr, "freeing dynamic-allocated array %x\n", globals);
                delete[] globals;
            }
        }
    } global_refs;

    using namespace v8;
    extern Nan::Persistent <Object> bindings;
    extern Nan::Persistent <ObjectTemplate> javaObjectWrap;


    /**
    * reads and clears java exception info
    *
    * @param buf reference of the buffer to be returned, maybe non-null if pre-allocated
    * @param len reference of the bufLen to be returned, should be zero if buf is not pre-allocated, or size of the buf
    */
    void getJavaException(JNIEnv * env, jchar * &buf, size_t & len);

    class JavaObject: public Nan::ObjectWrap {
    private:
        inline JavaObject(JavaVM *jvm, JNIEnv *env, jobject obj) : Nan::ObjectWrap(),
                jvm(jvm), env(env), _obj(obj) {
            Wrap(Nan::New(javaObjectWrap)->NewInstance());
            MakeWeak();
        }
    public:
        JavaVM *jvm;
        JNIEnv *env;
        jobject _obj;

        static inline Local <Value> wrap(JavaVM *jvm, JNIEnv *env, jobject obj) {
            if (!obj) return Local<Value>();
            JavaObject *ptr = new JavaObject(jvm, env, env->NewGlobalRef(obj));
            env->DeleteLocalRef(obj);
            return ptr->handle();
        }

        static inline JavaObject* Unwrap(Local<Value> any) {
            return Nan::ObjectWrap::Unwrap<JavaObject>(any.As<Object>());
        }

        template<typename s>
        static inline s unwrap(Local<Value> any) {
            return static_cast<s>(Nan::ObjectWrap::Unwrap<JavaObject>(any.As<Object>())->_obj);
        }
    };

    class JavaMethod {
    private:
        char _argTypes[16];
    public:
        bool isStatic;
        jmethodID methodID;
        const char *argTypes;
        int args;
        char retType;

        inline JavaMethod(jmethodID methodID, bool isStatic) :
                isStatic(isStatic), methodID(methodID), argTypes(_argTypes) {
        }

        inline Local <External> wrap(Isolate *isolate) {
            return Nan::New<External>(this);
        }
    };

    inline Local <String> cast(JNIEnv *env, Isolate *isolate, jstring javastr) {
        if (!javastr) {
            return Local<String>();
        }

        jsize strlen = env->GetStringLength(javastr);
        const jchar *chars = env->GetStringCritical(javastr, NULL);
        // Local <String> ret = Nan::New<String>(chars, strlen);
        // TODO: Nan does not support new string from two bytes with length.
        Local <String> ret = String::NewFromTwoByte(isolate, chars, v8::String::kNormalString, strlen);
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

    inline void ThrowJavaException(JNIEnv * env, Isolate * isolate) {
        const size_t BUF_SIZE = 128;
        jchar buf[BUF_SIZE]; // pre-allocated buffer, to be filled by `getJavaException'
        jchar *msg = buf;
        size_t msgLen = BUF_SIZE;
        getJavaException(env, msg, msgLen);
		Nan::ThrowError(Exception::Error(String::NewFromTwoByte(isolate, msg, v8::String::kNormalString, msgLen)));
        if (msgLen > BUF_SIZE) {
            delete[] msg;
        }
    }

    namespace vm {

        inline jobject unwrap(Local < Value > val) {
            Local <Object> obj = val->ToObject();
            Local <Value> handle = obj->Get(Nan::NewOneByteString(reinterpret_cast<const uint8_t*>("handle"), 6).ToLocalChecked());
            if (!handle->IsObject()) {
                return NULL;
            }
            return JavaObject::unwrap<jobject>(handle);

        }

        inline void parseValue(jvalue &val, const char type, Local <Value> arg, JNIEnv *env) {
            switch (type) {
                case '$': // convert to jstring
                    val.l = arg->IsNull() ? NULL : arg->IsObject() ? unwrap(arg) : cast(env, arg);
                    break;
                case 'L':
                    val.l = arg->IsObject() ? unwrap(arg) : arg->IsString() ? cast(env, arg) : NULL;
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
