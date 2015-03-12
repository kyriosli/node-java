#include <dlfcn.h>

#include "java.h"
#include "async.h"
#include<string.h>




namespace java {
	using namespace v8;

	const jchar* getJavaException(JNIEnv* env, int* len) {
		env->PushLocalFrame(0);
		jthrowable e = env->ExceptionOccurred();
		env->ExceptionClear();
		jclass cls = env->GetObjectClass(e);
		static jmethodID getName = env->GetMethodID(env->GetObjectClass(cls), "getName", "()Ljava/lang/String;");
		jstring clsName = (jstring) env->CallObjectMethod(cls, getName);

		jmethodID getMessage = env->GetMethodID(cls, "getMessage", "()Ljava/lang/String;");
		jstring message = (jstring) env->CallObjectMethod(e, getMessage);

		jsize nameLen = env->GetStringLength(clsName),
			msgLen = env->GetStringLength(message);

		jchar* buf = new jchar[nameLen + msgLen + 3];
		const jchar* chars = env->GetStringCritical(clsName, NULL);
		memcpy(buf, chars, nameLen << 1);
		buf[nameLen] = buf[nameLen + 2] = ' ';
		buf[nameLen + 1] = ':';
		env->ReleaseStringCritical(clsName, chars);

		chars = env->GetStringCritical(message, NULL);
		memcpy(buf + nameLen + 3, chars, msgLen << 1);
		env->ReleaseStringCritical(message, chars);
		
		if(len) *len = nameLen + msgLen + 3;
		env->PopLocalFrame(NULL);
		return buf;
	}


	void async::Task::execute() {
		JNIEnv* env;
		jint errno = vm->AttachCurrentThread((void**) &env, NULL);
		if(errno) {
			char buf[64];
			int count = sprintf(buf, "`AttachCurrentThread' failed with errno %d, This may cause memory leak!!!", errno);
			jchar* msg = new jchar[count];
			printw(msg, buf, count);
			reject(msg, count);
			return;
		}
		run(env); // this is virtual
		vm->DetachCurrentThread();
	}

	void async::Task::onFinish() {
		Isolate* isolate = Isolate::GetCurrent();
		HandleScope handle_scope(isolate);

		Local<Promise::Resolver> res = Local<Promise::Resolver>::New(isolate, resolver);

		if(resolved_type == 'e') {
			// reject
			Local<String> jsmsg = String::NewFromTwoByte(isolate, msg, String::kNormalString, msg_len);
			delete[] msg;
			res->Reject(Exception::Error(jsmsg));
		} else if(resolved_type == 'V') {
			res->Resolve(Undefined(isolate));
		} else if(resolved_type == '$' || resolved_type == 'L') {
			if(!value.l) {
				res->Resolve(Null(isolate));
			} else {
				JNIEnv* env;
				jint errno = GET_ENV(vm, env);
				if(errno) {
					char errMsg[64];
					int len = sprintf(errMsg, "`GetEnv' failed with error code: %d", errno);
					res->Reject(Exception::Error(String::NewFromUtf8(isolate, errMsg, String::kNormalString, len)));
				} else {
					res->Resolve(convert(resolved_type, isolate, env, value));
				}
			}
		} else {
			res->Resolve(convert(resolved_type, isolate, NULL, value));
		}
	}


	namespace vm {
		void createVm(const FunctionCallbackInfo<Value>& args) { // int argc, char*
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);

			// try to get a JavaVM that has already been created.
			JavaVM *jvm;
			
			jsize vmcount;
			JNI_GetCreatedJavaVMs(&jvm, 1, &vmcount);
			if(!vmcount) {
				// not found
				JNIEnv *env;
				JavaVMInitArgs vm_args;
				vm_args.version = JNI_VERSION;

				int argc = args[0]->Int32Value();
				String::Utf8Value argv(args[1]);

				char* ptr = *argv;

				JavaVMOption* options = new JavaVMOption[argc];
				for(int i = 0; i < argc; i++) {
					options[i].optionString = ptr;
					while(*ptr) ptr++; // ptr points to next "\0"
					ptr++;
				}

				vm_args.options = options;
				vm_args.nOptions = argc;

				jint errno = JNI_CreateJavaVM(&jvm, reinterpret_cast<void**>(&env), &vm_args);
				delete[] options;

				CHECK_ERRNO(errno, "CreateVM failed")
				
				if(env->ExceptionCheck()) {
					env->ExceptionDescribe();
					env->ExceptionClear();
				}
			}

			RETURN(External::New(isolate, jvm));
	    }

		class AsyncMainTask : public async::Task {
		private:
			jclass cls;
			jmethodID main;
			jobjectArray args;
		public:
			AsyncMainTask(JavaVM* vm, JNIEnv* env, jclass cls, jmethodID main, jobjectArray args, Isolate* isolate) :
				Task(vm, isolate),
				cls(static_cast<jclass>(env->NewGlobalRef(cls))),
				main(main),
				args(static_cast<jobjectArray>(env->NewGlobalRef(args))) {}


			void run (JNIEnv* env) {
				jvalue jargs;
				jargs.l = args;
				env->CallStaticVoidMethodA(cls, main, &jargs);
				// check exception

				if(env->ExceptionCheck()) {
					int len;
					const jchar* msg = getJavaException(env, &len);
					reject(msg, len);
				} else {
					resolve('V', jvalue());
				}
				
				env->DeleteGlobalRef(cls);
				env->DeleteGlobalRef(args);
			}

		};

		void runMain(const FunctionCallbackInfo<Value>& args) {// handle, string cls, array[] args, boolean async
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);

			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env;
			SAFE_GET_ENV(jvm, env)
			env->PushLocalFrame(128);

			jclass cls = env->FindClass(*String::Utf8Value(args[1]));
			if(!cls) { // exception occured
				ThrowJavaException(env, isolate);
				env->PopLocalFrame(NULL);
				return;
			}

			jmethodID main = env->GetStaticMethodID(cls, "main", "([Ljava/lang/String;)V");
			if(!main) {
				THROW("main method not found");
				env->PopLocalFrame(NULL);
				return;
			}
			// convert arguments
			Local<Array> argv = Local<Array>::Cast(args[2]);
			int argc = argv->Length();

			static jclass JavaLangString = (jclass) env->NewGlobalRef(env->FindClass("java/lang/String"));
			jobjectArray javaArgs = env->NewObjectArray(argc, JavaLangString, NULL);

			for(int i=0; i<argc; i++) {
				env->SetObjectArrayElement(javaArgs, i, cast(env, argv->Get(i)));
			}

			if(args[3]->BooleanValue()) { // async
				AsyncMainTask* task = new AsyncMainTask(jvm, env, cls, main, javaArgs, isolate);
				task->enqueue();
				RETURN(task->promise(isolate));
			} else {
				jvalue jargs;
				jargs.l = javaArgs;
				env->CallStaticVoidMethodA(cls, main, &jargs);

				// check exception
				if(env->ExceptionCheck()) {
					ThrowJavaException(env, isolate);
				}
			}
			env->PopLocalFrame(NULL);
		}

		void findClass(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env;
			SAFE_GET_ENV(jvm, env)

			jclass cls = env->FindClass(*String::Utf8Value(args[1]));
			if(!cls) { // exception occured
				ThrowJavaException(env, isolate);
				return;
			}
			RETURN(JavaObject::wrap(env, cls, isolate));
			
		}

		// getClass(vm, obj)
		void getClass(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env;
			SAFE_GET_ENV(jvm, env)
			jobject obj = UNWRAP(args[1], jobject); // a global reference
			RETURN(JavaObject::wrap(env, env->GetObjectClass(obj), isolate));
		}


		// findMethod(vm, cls, signature, isStatic)
		void findMethod(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env;
			SAFE_GET_ENV(jvm, env)
			jclass cls = UNWRAP(args[1], jclass); // a global reference


			bool isStatic = args[3]->BooleanValue();
			String::Utf8Value signature(args[2]);
			const char* ptr = *signature;
			char name[128];

			char* pname = name;
			while(*ptr != '(') {
				*pname = *ptr;
				ptr++;
				pname++;
			}
			*pname = 0;
			fprintf(stderr, "findMethod(%s, %s)\n", name, ptr);

			jmethodID methodID = isStatic ?
				env->GetStaticMethodID(cls, name, ptr) :
				env->GetMethodID(cls, name, ptr);

			if(!methodID) {
				char buf[128];
				sprintf(buf, "method `%s' not found.", *signature);
				THROW(buf);
				return;
			}

			JavaMethod* method = new JavaMethod(isolate, methodID, isStatic);
			char* argTypes = (char*) method->argTypes;
			int argc = 0, capacity = 16;

			ptr++;
			for(;;) {
				char type = *ptr;
				if(type == ')') break;
				if(type == '[') {
					type = 'L';
					do {
						ptr++;
					} while(*ptr == '[');
				} else if(type == 'L' && !strncmp(ptr, "Ljava/lang/String;", 18)) {
					type = '$';
					ptr += 17;
				}
				if(*ptr == 'L') {
					// object or object array
					do {
						ptr++;
					} while(*ptr != ';');
				}
				ptr++; // skip current arg

				if(argc == capacity) {
					capacity = capacity == 16 ? 256 : capacity << 1;	
					char* tmp = new char[capacity];
					strncpy(tmp, argTypes, argc);
					if(capacity > 256) delete[] argTypes;
					argTypes = tmp;
				}
				argTypes[argc++] = type;
			}
			argTypes[argc] = 0;
			fprintf(stderr, "argType of %s is %s\n", *signature, argTypes);


			// get ret type
			ptr++;
			char retType = *ptr;
			if(retType == '[') {
				retType = 'L';
			}

			method->args = argc;
			method->argTypes = argTypes;
			method->retType = retType;

			RETURN(method->wrap(isolate));

		}

		

		// newInstance(vm, cls, method, ...args)
		void newInstance(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env;
			SAFE_GET_ENV(jvm, env)
			jclass cls = UNWRAP(args[1], jclass); // a global reference
			JavaMethod* method = GET_PTR(args, 2, JavaMethod);
			jvalue values[256];
			env->PushLocalFrame(128);
			parseValues(env, method, args, values, 3);

			jobject ref = env->NewObjectA(cls, methodID, values);
			RETURN(JavaObject::wrap(env, ref, isolate));
			env->PopLocalFrame(NULL);
		}

		class AsyncInvokeTask : public async::Task {
		private:
			bool isStatic;
			char retType;
			jobject obj;
			jmethodID methodID;
			jvalue values[256];
			jobject globalRefs[256];
			int globalRefCount;
		public:

			AsyncInvokeTask(JavaVM* vm, Isolate* isolate, JNIEnv* env, jobject obj, jmethodID methodID,
				const char* types, Local<Array> args, char retType, bool isStatic) :
				Task(vm, isolate), obj(env->NewGlobalRef(obj)), methodID(methodID), retType(retType), isStatic(isStatic),
				globalRefCount(1) {
				globalRefs[0] = obj;

				int count = args->Length();
				for(int i = 0; i < count; i++) {
					jvalue& val = values[i];
					parseValue(val, types[i], args->Get(i), env);
					if(types[i] == '$' || types[i] == 'L') {
						val.l = globalRefs[globalRefCount++] = env->NewGlobalRef(val.l);
					}
				}
			}

			void run(JNIEnv* env) {
				env->PushLocalFrame(128);
				jvalue ret;
				invoke(ret, isStatic, retType, env, obj, methodID, values);

				resolve(retType, ret);

				env->PopLocalFrame(NULL);


				for(int i = 0; i < globalRefCount; i++)
					env->DeleteGlobalRef(globalRefs[i]);
			}
		};

		
		
		// invokeAsync(vm, obj, methodID, types, args, retType, isStatic)
		void invokeAsync(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env;
			SAFE_GET_ENV(jvm, env)

			jobject obj = UNWRAP(args[1], jobject); // a global reference
			jmethodID methodID = GET_PTR(args, 2, jmethodID);
			char retType = **String::Utf8Value(args[5]);
			bool isStatic = args[6]->BooleanValue();

			async::Task* task = new AsyncInvokeTask(jvm, isolate, env, UNWRAP(args[1], jobject), methodID, *String::Utf8Value(args[3]), Local<Array>::Cast(args[4]), retType, isStatic);
			task->enqueue();
			RETURN(task->promise(isolate));
		}

		// invoke(vm, obj, methodID, types, args, retType, isStatic)
		void invoke(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env;
			SAFE_GET_ENV(jvm, env)
			jobject obj = UNWRAP(args[1], jobject); // a global reference
			jmethodID methodID = GET_PTR(args, 2, jmethodID);
			String::Utf8Value signature(args[3]);
			const char* ptr = *signature;

			char retType = ptr[0];
			bool isStatic = ptr[1] == '#';

			jvalue values[256];
			env->PushLocalFrame(128);
			parseValues(env, values, ptr + 2, Local<Array>::Cast(args[4]));

			jvalue ret;

			java::invoke(ret, isStatic, retType, env, obj, methodID, values);

			if(retType != 'V') {
				if((retType == '$' || retType == 'L') && !ret.l) {
					args.GetReturnValue().SetNull();
				} else {
					RETURN(convert(retType, isolate, env, ret));
				}
			}

			env->PopLocalFrame(NULL);
		}

		void dispose(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);

			JavaVM* jvm = GET_JVM(args);
			jint errno = jvm->DestroyJavaVM();
			CHECK_ERRNO(errno, "destroyVm failed");
		}

		void link(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);

			void* handle = dlopen(*String::Utf8Value(args[0]), RTLD_LAZY | RTLD_GLOBAL);
			if(!handle) {
				THROW(dlerror());
				return;
			}
			
		}
	}


	// init bindings
	void init(Handle<Object> exports) {
		Isolate* isolate = Isolate::GetCurrent(); 
		HandleScope handle_scope(isolate);
#define REGISTER(name)	NODE_SET_METHOD(exports, #name, vm::name);
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

NODE_MODULE(java, java::init)
