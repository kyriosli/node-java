#include <dlfcn.h>

#include "java.h"
#include "async.h"
#include<string.h>


namespace java {
    using namespace v8;

    namespace vm {
        void createVm(const FunctionCallbackInfo <Value> &args) { // int argc, char*
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            // try to get a JavaVM that has already been created.
            JavaVM *jvm;

            // first, try to find a vm that has already been created.
            jsize vmcount;
            JNI_GetCreatedJavaVMs(&jvm, 1, &vmcount);
            if (!vmcount) { // not found, but that's ok, let's create one
                JNIEnv *env;
                JavaVMInitArgs vm_args;
                vm_args.version = JNI_VERSION;

                int argc = args[0]->Int32Value();
                String::Utf8Value argv(args[1]);

                char *ptr = *argv;

                JavaVMOption *options = new JavaVMOption[argc];
                for (int i = 0; i < argc; i++) {
                    options[i].optionString = ptr;
                    while (*ptr) ptr++; // ptr points to next "\0"
                    ptr++;
                }

                vm_args.options = options;
                vm_args.nOptions = argc;

                jint errno = JNI_CreateJavaVM(&jvm, reinterpret_cast<void **>(&env), &vm_args);
                delete[] options;

                CHECK_ERRNO(errno, "CreateVM failed")

                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
            }

            RETURN(External::New(isolate, jvm));
        }

        /**
        * runMainAsync的实现
        */
        class AsyncMainTask : public async::Task {
        private:
            jclass cls;
            jmethodID main;
            jobjectArray args;
        public:
            inline AsyncMainTask(JavaVM *vm, JNIEnv *env, jclass cls, jmethodID main, jobjectArray args, Isolate *isolate)
                    : Task(vm, isolate), cls(static_cast<jclass>(env->NewGlobalRef(cls))),
                      main(main), args(static_cast<jobjectArray>(env->NewGlobalRef(args))) {
            }

            // overrides pure abstract method async::Task::run(JNIEnv *)
            void run(JNIEnv *env) {
                jvalue jargs;
                jargs.l = args;
                env->CallStaticVoidMethodA(cls, main, &jargs);
                // check exception

                if (env->ExceptionCheck()) {
                    int len;
                    const jchar *msg = getJavaException(env, &len);
                    reject(msg, len);
                } else {
                    resolve('V', jvalue());
                }

                env->DeleteGlobalRef(cls);
                env->DeleteGlobalRef(args);
            }

        }; // End class AsyncMainTask

        // runMain(vm, cls, args, async)
        void runMain(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            JavaVM *jvm = GET_PTR(args, 0, JavaVM *);

            JNIEnv *env;
            SAFE_GET_ENV(jvm, env)
            env->PushLocalFrame(128);

            jclass cls = env->FindClass(*String::Utf8Value(args[1]));
            if (!cls) { // exception occured
                ThrowJavaException(env, isolate);
                env->PopLocalFrame(NULL);
                return;
            }

            jmethodID main = env->GetStaticMethodID(cls, "main", "([Ljava/lang/String;)V");
            if (!main) {
                THROW("main method not found");
                env->PopLocalFrame(NULL);
                return;
            }
            // convert arguments
            Local <Array> argv = Local<Array>::Cast(args[2]);
            int argc = argv->Length();

            static jclass JavaLangString = (jclass) env->NewGlobalRef(env->FindClass("java/lang/String"));
            jobjectArray javaArgs = env->NewObjectArray(argc, JavaLangString, NULL);

            for (int i = 0; i < argc; i++) {
                env->SetObjectArrayElement(javaArgs, i, cast(env, argv->Get(i)));
            }

            if (args[3]->BooleanValue()) { // async == true
                AsyncMainTask *task = new AsyncMainTask(jvm, env, cls, main, javaArgs, isolate);
                task->enqueue();
                RETURN(task->promise(isolate));
            } else {
                jvalue jargs;
                jargs.l = javaArgs;
                env->CallStaticVoidMethodA(cls, main, &jargs);

                // check exception
                if (env->ExceptionCheck()) {
                    ThrowJavaException(env, isolate);
                }
            }
            env->PopLocalFrame(NULL);
        }

        // findClass(vm, name, classCache)
        void findClass(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaVM *jvm = GET_PTR(args, 0, JavaVM *);

            JNIEnv *env;
            SAFE_GET_ENV(jvm, env)

            jclass cls = env->FindClass(*String::Utf8Value(args[1]));
            if (!cls) { // exception occured
                ThrowJavaException(env, isolate);
                return;
            }
            RETURN(JavaObject::wrap(jvm, env, cls, isolate));
        }

        // getClass(obj, cache)
        void getClass(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject*);

            JNIEnv *env = handle->env;

            env->PushLocalFrame(16);
            jclass cls = env->GetObjectClass(handle->_obj);
            static jmethodID GetClassName = env->GetMethodID(env->GetObjectClass(cls), "getName", "()Ljava/lang/String;");
            jstring name = (jstring) env->CallObjectMethod(cls, GetClassName);

            jsize strlen = env->GetStringLength(name);
            const jchar *chars = env->GetStringCritical(name, NULL);
            jchar nameChars[256];

            for (int i = 0; i < strlen; i++) {
                jchar ch = chars[i];
                nameChars[i] = ch == '.' ? '/' : ch;
            }
            Local <String> clsName = String::NewFromTwoByte(isolate, nameChars, String::kNormalString, strlen);
            env->ReleaseStringCritical(name, chars);


            Local <Object> classCache = Local<Object>::Cast(args[1]);

            bool hasKey = classCache->Has(clsName);

            Local <Array> ret = Array::New(isolate, 2);
            ret->Set(0, clsName);

            if (!hasKey) {
                ret->Set(1, JavaObject::wrap(handle->jvm, env, env->GetObjectClass(handle->_obj), isolate));
            }

            RETURN(ret);
            env->PopLocalFrame(NULL);
        }


        // findMethod(cls, signature, isStatic)
        void findMethod(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject*);

            JNIEnv *env = handle->env;
            jclass cls = (jclass) handle->_obj; // a global reference


            bool isStatic = args[2]->BooleanValue();
            String::Utf8Value signature(args[1]);
            const char *ptr = *signature;
            char name[128];

            char *pname = name;
            while (*ptr != '(') {
                *pname = *ptr;
                ptr++;
                pname++;
            }
            *pname = 0;
            // fprintf(stderr, "findMethod(%s, %s)\n", name, ptr);

            jmethodID methodID = isStatic ?
                    env->GetStaticMethodID(cls, name, ptr) :
                    env->GetMethodID(cls, name, ptr);

            if (!methodID) {
                char buf[128];
                sprintf(buf, "method `%s' with signature `%s' not found.", name, ptr);
                THROW(buf);
                env->ExceptionClear();
                return;
            }

            JavaMethod *method = new JavaMethod(methodID, isStatic);
            char *argTypes = (char *) method->argTypes;
            int argc = 0, capacity = 16;

            ptr++;
            for (; ;) {
                char type = *ptr;
                if (type == ')') break;
                if (type == '[') {
                    type = 'L';
                    do {
                        ptr++;
                    } while (*ptr == '[');
                } else if (type == 'L' && !strncmp(ptr, "Ljava/lang/String;", 18)) {
                    type = '$';
                    ptr += 17;
                }
                if (*ptr == 'L') {
                    // object or object array
                    do {
                        ptr++;
                    } while (*ptr != ';');
                }
                ptr++; // skip current arg

                if (argc == capacity) {
                    capacity = capacity == 16 ? 256 : capacity << 1;
                    char *tmp = new char[capacity];
                    strncpy(tmp, argTypes, argc);
                    if (capacity > 256) delete[] argTypes;
                    argTypes = tmp;
                }
                argTypes[argc++] = type;
            }
            argTypes[argc] = 0;
            // fprintf(stderr, "argType of %s is %s\n", *signature, argTypes);


            // get ret type
            ptr++;
            char retType = *ptr;
            if (retType == '[') {
                retType = 'L';
            } else if (retType == 'L' && !strncmp(ptr, "Ljava/lang/String;", 18)) {
                retType = '$';
            }

            method->args = argc;
            method->argTypes = argTypes;
            method->retType = retType;

            RETURN(method->wrap(isolate));

        }


        // newInstance(cls, method, args)
        void newInstance(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject*);
            JavaMethod *method = GET_PTR(args, 1, JavaMethod*);

            JNIEnv *env = handle->env;
            jclass cls = (jclass) handle->_obj; // a global reference

            jvalue values[256];
            env->PushLocalFrame(128);
            parseValues(env, method, args[2], values);

            jobject ref = env->NewObjectA(cls, method->methodID, values);
            RETURN(JavaObject::wrap(handle->jvm, env, ref, isolate));
            env->PopLocalFrame(NULL);
        }

        class AsyncInvokeTask : public async::Task {
        public:
            jobject obj;
            JavaMethod *method;
            jvalue values[256];
            jobject globalRefs[256];
            int globalRefCount;

            AsyncInvokeTask(Isolate *isolate, JavaObject *handle, JavaMethod *method) :
                    Task(handle->jvm, isolate),
                    obj(handle->env->NewGlobalRef(handle->_obj)),
                    method(method),
                    globalRefCount(1) {
                globalRefs[0] = obj;

            }

            void run(JNIEnv *env) {
                env->PushLocalFrame(128);
                jvalue ret;
                java::invoke(env, obj, method, values, ret);

                resolve(method->retType, ret);

                env->PopLocalFrame(NULL);


                for (int i = 0; i < globalRefCount; i++)
                    env->DeleteGlobalRef(globalRefs[i]);
            }
        };


        // invokeAsync(obj, method, args)
        void invokeAsync(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject*);
            JavaMethod *method = GET_PTR(args, 1, JavaMethod*);

            JNIEnv *env = handle->env;

            env->PushLocalFrame(128);

            AsyncInvokeTask *task = new AsyncInvokeTask(isolate, handle, method);

            Local <Object> arguments = Local<Object>::Cast(args[2]);

            for (int count = method->args, i = 0; i < count; i++) {
                jvalue &val = task->values[i];
                char argType = method->argTypes[i];
                parseValue(val, argType, arguments->Get(i + 1), env);
                if (argType == '$' || argType == 'L') {
                    val.l = task->globalRefs[task->globalRefCount++] = env->NewGlobalRef(val.l);
                }
            }

            task->enqueue();
            RETURN(task->promise(isolate));
            env->PopLocalFrame(NULL);
        }

        // invoke(obj, method, args)
        void invoke(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject*);
            JavaMethod *method = GET_PTR(args, 1, JavaMethod*);

            JNIEnv *env = handle->env;
            jvalue values[256];
            env->PushLocalFrame(128);
            parseValues(env, method, args[2], values);

            jvalue ret;
            java::invoke(env, handle->_obj, method, values, ret);

            // check exception
            if (env->ExceptionCheck()) {
                ThrowJavaException(env, isolate);
            } else {
                char retType = method->retType;
                if (retType != 'V') {
                    if ((retType == '$' || retType == 'L') && !ret.l) {
                        args.GetReturnValue().SetNull();
                    } else {
                        RETURN(convert(retType, isolate, handle->jvm, env, ret));
                    }
                }
            }

            env->PopLocalFrame(NULL);
        }

        void dispose(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            JavaVM *jvm = GET_PTR(args, 0, JavaVM *);
            jint errno = jvm->DestroyJavaVM();
            CHECK_ERRNO(errno, "destroyVm failed");
        }

        void link(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            void *handle = dlopen(*String::Utf8Value(args[0]), RTLD_LAZY | RTLD_GLOBAL);
            if (!handle) {
                THROW(dlerror());
                return;
            }

        }
    }


    // init bindings
    void init(Handle < Object > exports) {
        Isolate *isolate = Isolate::GetCurrent();
        HandleScope handle_scope(isolate);
#define REGISTER(name)    NODE_SET_METHOD(exports, #name, vm::name);
        REGISTER(link);
        REGISTER(createVm);
        REGISTER(dispose);
        REGISTER(runMain);
        REGISTER(findClass);
        REGISTER(findMethod);
        REGISTER(newInstance);
        REGISTER(invoke);
        REGISTER(invokeAsync);
        REGISTER(getClass);
#undef REGISTER
    }
}

#define INIT(module)    NODE_MODULE(module, module::init)
INIT(java)
#undef INIT
