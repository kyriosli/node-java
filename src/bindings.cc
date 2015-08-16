#include <dlfcn.h>

#include "java.h"
#include "async.h"
#include "java_native.h"
#include<string.h>

namespace java {


    namespace vm {

        // link(path, verbose)
        NAN_METHOD(link) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            void *handle = dlopen(*String::Utf8Value(info[0]), RTLD_LAZY | RTLD_GLOBAL);
            if (!handle) {
                return Nan::ThrowError(dlerror());
            }

            verbose = info[1]->BooleanValue();
            native::init();

        }

        NAN_METHOD(createVm) { // int argc, char*
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

                int argc = info[0]->Int32Value();
                String::Utf8Value argv(info[1]);

                char *ptr = *argv;

                JavaVMOption *options = new JavaVMOption[argc];
                for (int i = 0; i < argc; i++) {
                    options[i].optionString = ptr;
                    while (*ptr) ptr++; // ptr points to next "\0"
                    ptr++;
                }

                vm_args.options = options;
                vm_args.nOptions = argc;

                jint err = JNI_CreateJavaVM(&jvm, reinterpret_cast<void **>(&env), &vm_args);
                delete[] options;

                CHECK_ERRNO(err, "CreateVM failed")

                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
            }

            info.GetReturnValue().Set(External::New(isolate, jvm));
        }


        // runMain(vm, cls, args, async)
        NAN_METHOD(runMain) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            JavaVM *jvm = GET_PTR(info, 0, JavaVM *);

            JNIEnv *env;
            SAFE_GET_ENV(jvm, env)
            env->PushLocalFrame(128);

            jclass cls = env->FindClass(*String::Utf8Value(info[1]));
            if (!cls) { // exception occured
                ThrowJavaException(env, isolate);
                env->PopLocalFrame(NULL);
                return;
            }

            jmethodID main = env->GetStaticMethodID(cls, "main", "([Ljava/lang/String;)V");
            if (!main) {
                env->PopLocalFrame(NULL);
                return Nan::ThrowError("main method not found");
            }
            // convert arguments
            Local <Array> argv = Local<Array>::Cast(info[2]);
            int argc = argv->Length();

            static jclass JavaLangString = (jclass) env->NewGlobalRef(env->FindClass("java/lang/String"));
            jobjectArray javaArgs = env->NewObjectArray(argc, JavaLangString, NULL);

            for (int i = 0; i < argc; i++) {
                env->SetObjectArrayElement(javaArgs, i, cast(env, argv->Get(i)));
            }

            if (info[3]->BooleanValue()) { // async == true
                async::MainTask *task = new async::MainTask(jvm, env, cls, main, javaArgs, isolate);
                info.GetReturnValue().Set(task->promise());
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
        NAN_METHOD(findClass) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaVM *jvm = GET_PTR(info, 0, JavaVM *);

            JNIEnv *env;
            SAFE_GET_ENV(jvm, env)

            jclass cls = env->FindClass(*String::Utf8Value(info[1]));
            if (!cls) { // exception occured
                ThrowJavaException(env, isolate);
                return;
            }
            info.GetReturnValue().Set(JavaObject::wrap(jvm, env, cls));
        }

        // getClass(obj, cache, self)
        NAN_METHOD(getClass) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = JavaObject::Unwrap(info[0]);

            JNIEnv *env = handle->env;

            env->PushLocalFrame(16);
            jclass cls = info[2]->BooleanValue() ? reinterpret_cast<jclass>(handle->_obj) : env->GetObjectClass(handle->_obj);
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


            Local <Object> classCache = Local<Object>::Cast(info[1]);

            bool hasKey = classCache->Has(clsName);

            Local <Array> ret = Array::New(isolate, 2);
            ret->Set(0, clsName);

            if (!hasKey) {
                ret->Set(1, JavaObject::wrap(handle->jvm, env, env->GetObjectClass(handle->_obj)));
            }

            info.GetReturnValue().Set(ret);
            env->PopLocalFrame(NULL);
        }

        // findField(cls, name, type, isStatic)
        NAN_METHOD(findField) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = JavaObject::Unwrap(info[0]);

            JNIEnv *env = handle->env;
            jclass cls = (jclass) handle->_obj; // a global reference

            String::Utf8Value Name(info[1]);
            String::Utf8Value Type(info[2]);

            const char *pname = *Name, *ptype = *Type;

            bool isStatic = info[3]->BooleanValue();

            jfieldID fieldID = isStatic ?
                    env->GetStaticFieldID(cls, pname, ptype) :
                    env->GetFieldID(cls, pname, ptype);

            if (!fieldID) {
                char buf[128];
                sprintf(buf, "field `%s' with type `%s' not found.", pname, ptype);
                env->ExceptionClear();
                return Nan::ThrowError(buf);
            }

            char type = ptype[0];

            if (type == '[') {
                type = 'L';
            } else if (type == 'L' && !strncmp(ptype, "Ljava/lang/String;", 18)) {
                type = '$';
            }

            long long result = reinterpret_cast<long long>(fieldID) | static_cast<long long>(type) << 40 | static_cast<long long>(isStatic) << 48;

            info.GetReturnValue().Set(Number::New(isolate, result));
        }

        // get(handle, field)
        NAN_METHOD(get) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = JavaObject::Unwrap(info[0]);

            JNIEnv *env = handle->env;
            jobject obj = handle->_obj; // a global reference

            long long field = info[1]->NumberValue();
            bool isStatic = field >> 48;
            char type = field >> 40;
            jfieldID fieldID = reinterpret_cast<jfieldID>(field & 0xffffffffff);

            jvalue val;
            if (isStatic) {
                jclass cls = (jclass) obj;
                switch (type) {
                    case 'Z':
                        val.z = env->GetStaticBooleanField(cls, fieldID);
                        break;
                    case 'B':
                        val.b = env->GetStaticByteField(cls, fieldID);
                        break;
                    case 'C':
                        val.c = env->GetStaticCharField(cls, fieldID);
                        break;
                    case 'S':
                        val.s = env->GetStaticShortField(cls, fieldID);
                        break;
                    case 'I':
                        val.i = env->GetStaticIntField(cls, fieldID);
                        break;
                    case 'F':
                        val.f = env->GetStaticFloatField(cls, fieldID);
                        break;
                    case 'J':
                        val.j = env->GetStaticLongField(cls, fieldID);
                        break;
                    case 'D':
                        val.d = env->GetStaticDoubleField(cls, fieldID);
                        break;
                    case '$':
                    case 'L':
                        val.l = env->GetStaticObjectField(cls, fieldID);
                        break;
                }
            } else {
                switch (type) {
                    case 'Z':
                        val.z = env->GetBooleanField(obj, fieldID);
                        break;
                    case 'B':
                        val.b = env->GetByteField(obj, fieldID);
                        break;
                    case 'C':
                        val.c = env->GetCharField(obj, fieldID);
                        break;
                    case 'S':
                        val.s = env->GetShortField(obj, fieldID);
                        break;
                    case 'I':
                        val.i = env->GetIntField(obj, fieldID);
                        break;
                    case 'F':
                        val.f = env->GetFloatField(obj, fieldID);
                        break;
                    case 'J':
                        val.j = env->GetLongField(obj, fieldID);
                        break;
                    case 'D':
                        val.d = env->GetDoubleField(obj, fieldID);
                        break;
                    case '$':
                    case 'L':
                        val.l = env->GetObjectField(obj, fieldID);
                        break;
                }
            }

            info.GetReturnValue().Set(convert(type, isolate, handle->jvm, env, val));

        }

        // set(handle, field, value)
        NAN_METHOD(set) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = JavaObject::Unwrap(info[0]);

            JNIEnv *env = handle->env;
            jobject obj = handle->_obj; // a global reference

            long long field = info[1]->NumberValue();
            bool isStatic = field >> 48;
            char type = field >> 40;
            jfieldID fieldID = reinterpret_cast<jfieldID>(field & 0xffffffffff);

            jvalue val;
            parseValue(val, type, info[2], env);

            if (isStatic) {
                jclass cls = (jclass) obj;
                switch (type) {
                    case 'Z':
                        env->SetStaticBooleanField(cls, fieldID, val.z);
                        break;
                    case 'B':
                        env->SetStaticByteField(cls, fieldID, val.b);
                        break;
                    case 'C':
                        env->SetStaticCharField(cls, fieldID, val.c);
                        break;
                    case 'S':
                        env->SetStaticShortField(cls, fieldID, val.s);
                        break;
                    case 'I':
                        env->SetStaticIntField(cls, fieldID, val.i);
                        break;
                    case 'F':
                        env->SetStaticFloatField(cls, fieldID, val.f);
                        break;
                    case 'J':
                        env->SetStaticLongField(cls, fieldID, val.j);
                        break;
                    case 'D':
                        env->SetStaticDoubleField(cls, fieldID, val.d);
                        break;
                    case '$':
                    case 'L':
                        env->SetStaticObjectField(cls, fieldID, val.l);
                        break;
                }
            } else {
                switch (type) {
                    case 'Z':
                        env->SetBooleanField(obj, fieldID, val.z);
                        break;
                    case 'B':
                        env->SetByteField(obj, fieldID, val.b);
                        break;
                    case 'C':
                        env->SetCharField(obj, fieldID, val.c);
                        break;
                    case 'S':
                        env->SetShortField(obj, fieldID, val.s);
                        break;
                    case 'I':
                        env->SetIntField(obj, fieldID, val.i);
                        break;
                    case 'F':
                        env->SetFloatField(obj, fieldID, val.f);
                        break;
                    case 'J':
                        env->SetLongField(obj, fieldID, val.j);
                        break;
                    case 'D':
                        env->SetDoubleField(obj, fieldID, val.d);
                        break;
                    case '$':
                    case 'L':
                        env->SetObjectField(obj, fieldID, val.l);
                        break;
                }
            }
        }


        // findMethod(cls, signature, isStatic)
        NAN_METHOD(findMethod) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = JavaObject::Unwrap(info[0]);

            JNIEnv *env = handle->env;
            jclass cls = (jclass) handle->_obj; // a global reference


            bool isStatic = info[2]->BooleanValue();
            String::Utf8Value signature(info[1]);
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
                env->ExceptionClear();
                return Nan::ThrowError(buf);
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

            info.GetReturnValue().Set(method->wrap(isolate));

        }


        // newInstance(cls, method, args)
        NAN_METHOD(newInstance) {
            JavaObject *handle = JavaObject::Unwrap(info[0]);
            JavaMethod *method = GET_PTR(info, 1, JavaMethod *);

            JNIEnv *env = handle->env;
            jclass cls = (jclass) handle->_obj; // a global reference

            jvalue values[256];
            env->PushLocalFrame(128);
            parseValues(env, method, info[2], values);

            jobject ref = env->NewObjectA(cls, method->methodID, values);
            info.GetReturnValue().Set(JavaObject::wrap(handle->jvm, env, ref));
            env->PopLocalFrame(NULL);
        }


        // invokeAsync(obj, method, args)
        NAN_METHOD(invokeAsync) {
            Isolate *isolate = Isolate::GetCurrent();
            JavaObject *handle = JavaObject::Unwrap(info[0]);
            JavaMethod *method = GET_PTR(info, 1, JavaMethod *);

            JNIEnv *env = handle->env;

            env->PushLocalFrame(128);

            async::InvokeTask *task = new async::InvokeTask(isolate, handle, method);

            Local <Object> arguments = Local<Object>::Cast(info[2]);

            for (int count = method->args, i = 0; i < count; i++) {
                jvalue &val = task->values[i];
                char argType = method->argTypes[i];
                parseValue(val, argType, arguments->Get(i + 1), env);
                if (argType == '$' || argType == 'L') {
                    task->refs.push(env, val.l);
                }
            }

            info.GetReturnValue().Set(task->promise());
            env->PopLocalFrame(NULL);
        }

        // invoke(obj, method, args)
        NAN_METHOD(invoke) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = JavaObject::Unwrap(info[0]);
            JavaMethod *method = GET_PTR(info, 1, JavaMethod *);

            JNIEnv *env = handle->env;
            jvalue values[256];
            env->PushLocalFrame(128);
            parseValues(env, method, info[2], values);

            jvalue ret;
            java::invoke(env, handle->_obj, method, values, ret);
//            fprintf(stderr, "invoke completed with error %d\n", env->ExceptionCheck());
            // check exception
            if (env->ExceptionCheck()) {
                ThrowJavaException(env, isolate);
            } else {
                char retType = method->retType;
                if (retType != 'V') {
                    if ((retType == '$' || retType == 'L') && !ret.l) {
                        info.GetReturnValue().SetNull();
                    } else {
                        info.GetReturnValue().Set(convert(retType, isolate, handle->jvm, env, ret));
                    }
                }
            }

            env->PopLocalFrame(NULL);
        }

        NAN_METHOD(dispose) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            JavaVM *jvm = GET_PTR(info, 0, JavaVM *);
            jint err = jvm->DestroyJavaVM();
            CHECK_ERRNO(err, "destroyVm failed");
        }


        // defineClass(vm, name, buffer, natives)
        NAN_METHOD(defineClass) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);

            JavaVM *jvm = GET_PTR(info, 0, JavaVM *);

            JNIEnv *env;
            SAFE_GET_ENV(jvm, env)

            String::Utf8Value Name(info[1]);
            const char *pname = *Name;

            Local <ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(info[2]);

            ArrayBuffer::Contents contents = buffer->Externalize();

            const jbyte *ptr = static_cast<const jbyte *>(contents.Data());
            jclass cls = env->DefineClass(pname, NULL, ptr, contents.ByteLength());
            delete[] ptr;
            if (!cls) {
                ThrowJavaException(env, isolate);
                return;
            }

            Local <Array> natives = Local<Array>::Cast(info[3]);
            for (int i = 0, L = natives->Length(); i < L; i++) {
                String::Utf8Value Signature(natives->Get(i));
                const char *ptr = *Signature;
                // return type of method
                while (*ptr++ != ')');
                char retType = *ptr;
                // TODO: bulk this operation

//                fprintf(stderr, "native %d: %s return %c\n", i, *Signature, retType);

                JNINativeMethod callHelper = {(char *) ("_$CALLJS"), *Signature, native::callbacks[(int) retType]};
                jint err = env->RegisterNatives(cls, &callHelper, 1);
                CHECK_ERRNO(err, "RegisterNatives failed");
            }

            info.GetReturnValue().Set(JavaObject::wrap(jvm, env, cls));

        }

    }
}
