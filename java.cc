#include <dlfcn.h>

#include<jni.h>
#include<node.h>

#define	JNI_VERSION	JNI_VERSION_1_6
#define	THROW(msg)	isolate->ThrowException(Exception::Error(String::NewFromOneByte(isolate, reinterpret_cast<const uint8_t*>(msg))))
#define	GET_JVM(args)	reinterpret_cast<JavaVM*>(External::Cast(*args[0])->Value())
#define CHECK_ERRNO(errno, msg)	if(errno) {\
		uint8_t errMsg[128];\
		sprintf(reinterpret_cast<char*>(errMsg), msg " with error code: %d", errno);\
		THROW(errMsg);\
		return;\
	}

#define GET_ENV(jvm) NULL; {\
		jint errno = jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION);\
		CHECK_ERRNO(errno, "GetEnv failed")\
	}

namespace java {
	using namespace v8;

	inline Local<String> cast(JNIEnv* env, Isolate* isolate, jstring javastr) {
		if(!javastr) {
			return Null(isolate)->ToString();
		}
		
		// jsize strlen = env->GetStringLength(javastr);
		jboolean isCopy;
		const jchar* chars = env->GetStringCritical(javastr, &isCopy);
		Local<String> ret = String::NewFromTwoByte(isolate, chars);
		env->ReleaseStringCritical(javastr, chars);
		return ret;
	}

	inline jstring cast(JNIEnv* env, Local<Value> jsval) {
		String::Value val(jsval);
		int len = val.length();

		return env->NewString(*val, len);
	}

	static void wrapCallback(const WeakCallbackData<External, JNIEnv>& data) {
		JNIEnv* env = data.GetParameter();
//		Local<Object> obj = data.GetValue();
//		jobject ref = (jobject) obj->GetAlignedPointerFromInternalField(1);
//		env->DeleteGlobalRef(ref);
		static int count = 0;
		count++;
//		if(count % 100 == 0) {
			printf("%d references released\n", count);
//		}
	}

	inline Local<Value> wrap(JNIEnv* env, jobject obj, Isolate* isolate) {
		Local<External> ret = External::New(isolate, env->NewGlobalRef(obj));
		env->DeleteLocalRef(obj);
		Persistent<External>(isolate, ret).SetWeak(env, wrapCallback);
		return ret;
	}

	inline void ThrowJavaException(JNIEnv* env, Isolate* isolate) {
		env->PushLocalFrame(0);
		jthrowable e = env->ExceptionOccurred();
		env->ExceptionClear();
		jclass cls = env->GetObjectClass(e);
		static jmethodID getName = env->GetMethodID(env->GetObjectClass(cls), "getName", "()Ljava/lang/String;");
		jstring clsName = (jstring) env->CallObjectMethod(cls, getName);

		jmethodID getMessage = env->GetMethodID(cls, "getMessage", "()Ljava/lang/String;");
		jstring message = (jstring) env->CallObjectMethod(e, getMessage);

		Local<String> msg = String::Concat(String::Concat(cast(env, isolate, clsName), String::NewFromUtf8(isolate, " : ")), cast(env, isolate, message));
		isolate->ThrowException(Exception::Error(msg));
		env->PopLocalFrame(NULL);
	}

	namespace vm {
		void create(const FunctionCallbackInfo<Value>& args) { // int argc, char*
			Isolate* isolate = Isolate::GetCurrent();
			EscapableHandleScope handle_scope(isolate);

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

			args.GetReturnValue().Set(handle_scope.Escape(External::New(isolate, jvm)));
	    }

		void runMain(const FunctionCallbackInfo<Value>& args) {// handle, string cls, array[] args
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);

			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env = GET_ENV(jvm);
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
			jvalue vals[1];
			vals[0].l = javaArgs;

			env->CallStaticVoidMethodA(cls, main, vals);

			// check exception
			if(env->ExceptionCheck()) {
				ThrowJavaException(env, isolate);
			}

			env->PopLocalFrame(NULL);
		}

		void findClass(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			EscapableHandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env = GET_ENV(jvm);

			jclass cls = env->FindClass(*String::Utf8Value(args[1]));
			if(!cls) { // exception occured
				ThrowJavaException(env, isolate);
				return;
			}
			args.GetReturnValue().Set(handle_scope.Escape(wrap(env, cls, isolate)));
			
		}

		void dispose(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			HandleScope handle_scope(isolate);

			JavaVM* jvm = GET_JVM(args);
			jint errno = jvm->DestroyJavaVM();
			CHECK_ERRNO(errno, "destroyVm failed");
		}
	}


	// init bindings
	void init(Handle<Object> exports) {
		Isolate* isolate = Isolate::GetCurrent(); 
		HandleScope handle_scope(isolate);

		void* handle = dlopen(LD_LIB, RTLD_LAZY | RTLD_GLOBAL);
		if(!handle) {
			isolate->ThrowException(
				Exception::Error(String::NewFromOneByte(isolate, reinterpret_cast<const uint8_t*>(dlerror())))
			);
			return;
		}


		NODE_SET_METHOD(exports, "createVm", vm::create);
		NODE_SET_METHOD(exports, "dispose", vm::dispose);
		NODE_SET_METHOD(exports, "runMain", vm::runMain);
		NODE_SET_METHOD(exports, "findClass", vm::findClass);
	}
}

NODE_MODULE(java, java::init)
