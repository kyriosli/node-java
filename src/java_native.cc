#include "java.h"
#include "java_native.h"

#include<uv.h>
#include<string.h>

using namespace v8;

static void async_call(uv_async_t * handle);

namespace java {
    namespace native {
        void *callbacks[92];

        void execute(JNIEnv *env, Isolate *isolate, va_list &vl, jvalue *ret) {
            HandleScope handle_scope(isolate);

            va_arg(vl, jclass);
            Local <Object> receiver = Local<Object>::New(isolate, bindings);
            Local <Value> args[258];
            args[0] = cast(env, isolate, va_arg(vl, jstring)); // clsName
            jstring methodName = va_arg(vl, jstring),
                    methodSig = va_arg(vl, jstring);

            args[1] = String::Concat(cast(env, isolate, methodName), cast(env, isolate, methodSig));
            // methodName_sig


            const jchar *sig = env->GetStringChars(methodSig, NULL);
            const jchar *ptr = sig + 1;
            int argc = 2;

            for (; ;) {
                char type = *ptr++;
                if (type == ')') break;
//                    fprintf(stderr, "argument type %c\n", type);
                // skip to next argument

                Local <Value> &result = args[argc++];
                if (type == '[') {
                    while (*ptr++ == '[');
                    type = *ptr++;
                    if (type == 'L') {
                        while (*ptr++ != ';');
                    }
                    type = 'L';
                } else if (type == 'L') {
                    const char *javaString = "Ljava/lang/String;" + 1;
                    int m = 0;
                    for (; m < 17; m++) {
                        if (javaString[m] != ptr[m]) {
                            break;
                        }
                    }
                    if (m == 17) {
                        type = '$';
                        ptr += 17;
                    } else {
                        while (*ptr++ != ';');
                    }
                }

                jobject ptr;
                switch (type) {
                    case 'Z':
                        result = Boolean::New(isolate, va_arg(vl, jint));
                        break;
                    case 'B':
                    case 'S':
                    case 'C':
                    case 'I':
                        result = Integer::New(isolate, va_arg(vl, jint));
                        break;
                    case 'F':
                    case 'D':
                        result = Number::New(isolate, va_arg(vl, jdouble));
                        break;
                    case 'J':
                        result = Number::New(isolate, va_arg(vl, jlong));
                        break;
                    case 'L':
                        ptr = va_arg(vl, jobject);
                        if (ptr) {
                            result = External::New(isolate, ptr);
                        } else {
                            result = Null(isolate);
                        }
                        break;
                    case '$':
                        ptr = va_arg(vl, jobject);
                        if (ptr) {
                            result = cast(env, isolate, static_cast<jstring>(ptr));
                        } else {
                            result = Null(isolate);
                        }
                        break;
                }
            }

            char retType = *ptr;
            switch (retType) {
                case '[':
                    retType = 'L';
                    break;
                case 'L': {
                    const char *javaString = "Ljava/lang/String;";
                    int m = 0;
                    for (; m < 18; m++) {
                        if (javaString[m] != ptr[m]) {
                            break;
                        }
                    }
                    if (m == 18) {
                        retType = '$';
                    }
                }
                    break;
                case 'Z':
                case 'B':
                case 'C':
                case 'S':
                    retType = 'I';
                    break;
            }

            env->ReleaseStringChars(methodSig, sig);

            TryCatch tryCatch;
            Local <Value> result = Local<Object>::Cast(receiver->Get(0))->CallAsFunction(receiver, argc, args);
            if (tryCatch.HasCaught()) {
                env->ThrowNew(env->FindClass("java/lang/RuntimeException"), *String::Utf8Value(tryCatch.Message()->Get()));
            } else {
//                  fprintf(stderr, "execute completed with result %s\n", *String::Utf8Value(result));
                vm::parseValue(*ret, retType, result, env);
            }
        }

        typedef struct async_call_s {
            JNIEnv *env;
            uv_sem_t sem;
            va_list *vl;
            jvalue *ret;

            inline async_call_s(JNIEnv *env, va_list &vl, jvalue *ret) :
                    env(env), vl(&vl), ret(ret) {
                uv_async_t async;
                async.data = (void *) this;
                uv_async_init(uv_default_loop(), &async, async_call);
                uv_sem_init(&sem, 0);

                uv_async_send(&async);
                uv_sem_wait(&sem);
                uv_sem_destroy(&sem);
            }

            inline void call() {
                Isolate *isolate = Isolate::GetCurrent();
                execute(env, isolate, *vl, ret);
            }

        } async_call_t;
    }

}


static void async_call(uv_async_t * handle) {
    java::native::async_call_t &call = *reinterpret_cast<java::native::async_call_t *>(handle->data);
    call.call();

    uv_sem_post(&call.sem);
//    fprintf(stderr, "invoke async sem posted\n");
    uv_close((uv_handle_t *) handle, NULL);
}


static void callV8(JNIEnv * env, va_list & vl, jvalue * ret) {
    Isolate *isolate = Isolate::GetCurrent();
    if (isolate) {// called under v8
        java::native::execute(env, isolate, vl, ret);
    } else {// called outside v8
        java::native::async_call_t(env, vl, ret);
    }
    va_end(vl);
}

#define call(env)   va_list vl;\
    va_start(vl, env);\
    jvalue ret;\
    callV8(env, vl, &ret);

static void voidCallback(JNIEnv *env, ...) {
    call(env);
}

static jint intCallback(JNIEnv *env, ...) {
    call(env);
    return ret.i;
}

static jfloat floatCallback(JNIEnv *env, ...) {
    call(env);
    return ret.f;
}

static jdouble doubleCallback(JNIEnv *env, ...) {
    call(env);
    return ret.d;
}

static jlong longCallback(JNIEnv *env, ...) {
    call(env);
    return ret.j;
}

static jobject objectCallback(JNIEnv *env, ...) {
    call(env);
    return ret.l;
}

void java::native::init() {
    callbacks['V'] = (void *) voidCallback;
    callbacks['I'] = callbacks['Z'] = callbacks['B'] = callbacks['S'] = callbacks['C'] = (void *) intCallback;
    callbacks['F'] = (void *) floatCallback;
    callbacks['D'] = (void *) doubleCallback;
    callbacks['J'] = (void *) longCallback;

    callbacks['L'] = callbacks['['] = (void *) objectCallback;
}
