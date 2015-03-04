#include <dlfcn.h>

#include<jni.h>
#include<node.h>

#define	JNI_VERSION	JNI_VERSION_1_6
#define	THROW(msg)	isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate, msg)))
#define GET_PTR(args, idx, type)	static_cast<type>(External::Cast(*args[idx])->Value())
#define	GET_JVM(args)	GET_PTR(args, 0, JavaVM*)
#define CHECK_ERRNO(errno, msg)	if(errno) {\
		char errMsg[128];\
		sprintf(errMsg, msg " with error code: %d", errno);\
		THROW(errMsg);\
		return;\
	}

#define GET_ENV(jvm) NULL; {\
		jint errno = jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION);\
		CHECK_ERRNO(errno, "GetEnv failed")\
	}

#define RETURN(val)	args.GetReturnValue().Set(handle_scope.Escape(val))
#define UNWRAP(arg, type)	static_cast<type>(static_cast<JavaObject*>(External::Cast(*arg)->Value())->_obj)

namespace java {
	using namespace v8;

	typedef class JavaObject {
	private:
		Persistent<External> _ref;

		inline JavaObject (JNIEnv* env, Isolate* isolate, jobject obj) :
			_obj(obj),
			_ref(isolate, External::New(isolate, this)) {
			_ref.SetWeak(env, WeakCallback);
			_ref.MarkIndependent();
		}

		static void WeakCallback(const WeakCallbackData<External, JNIEnv>& data) {
			JNIEnv* env = data.GetParameter();
			JavaObject* ptr = (JavaObject*) data.GetValue()->Value();

			ptr->_ref.Reset();	
			env->DeleteGlobalRef(ptr->_obj);
			delete ptr;
		}
	public:
		jobject	_obj;
		static inline Local<Value> wrap (JNIEnv* env, jobject obj, Isolate* isolate) {
			JavaObject* ptr = new JavaObject(env, isolate, env->NewGlobalRef(obj));
			return Local<External>::New(isolate, ptr->_ref);
		}


	} JavaObject;

	inline Local<String> cast(JNIEnv* env, Isolate* isolate, jstring javastr) {
		if(!javastr) {
			return Null(isolate)->ToString();
		}
		
		jsize strlen = env->GetStringLength(javastr);
		const jchar* chars = env->GetStringCritical(javastr, NULL);
		Local<String> ret = String::NewFromTwoByte(isolate, chars, String::kNormalString, strlen);
		env->ReleaseStringCritical(javastr, chars);
		return ret;
	}

	inline jstring cast(JNIEnv* env, Local<Value> jsval) {
		String::Value val(jsval);
		int len = val.length();

		return env->NewString(*val, len);
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
		void createVm(const FunctionCallbackInfo<Value>& args) { // int argc, char*
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

			RETURN(External::New(isolate, jvm));
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
			RETURN(JavaObject::wrap(env, cls, isolate));
			
		}

		// getClass(vm, obj)
		void getClass(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			EscapableHandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env = GET_ENV(jvm);
			jobject obj = UNWRAP(args[1], jobject); // a global reference
			RETURN(JavaObject::wrap(env, env->GetObjectClass(obj), isolate));
		}


		// findMethod(vm, cls, name, signature, isStatic)
		void findMethod(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			EscapableHandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env = GET_ENV(jvm);
			jclass cls = UNWRAP(args[1], jclass); // a global reference

			jmethodID methodID = args[4]->BooleanValue() ? 
				env->GetStaticMethodID(cls, *String::Utf8Value(args[2]), *String::Utf8Value(args[3])) :
				env->GetMethodID(cls, *String::Utf8Value(args[2]), *String::Utf8Value(args[3]));
			RETURN(External::New(isolate, methodID));
		}

		inline void parseValues(JNIEnv* env, jvalue* values, Local<Value> vtypes, Local<Value> varr) {
			String::Utf8Value otypes(vtypes);
			const char* ctypes = *otypes;

			Local<Array> args = Local<Array>::Cast(varr);
			int count = args->Length();
			
			for(int i = 0; i < count; i++) {
				jvalue& val = values[i];
				Local<Value> arg = args->Get(i);
				switch(ctypes[i]) {
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
					val.c = arg->IsInt32() ? arg->Uint32Value() : **String::Utf8Value(arg);
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

		}


		// newInstance(vm, cls, methodID, types, args)
		void newInstance(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			EscapableHandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env = GET_ENV(jvm);
			jclass cls = UNWRAP(args[1], jclass); // a global reference
			jmethodID methodID = GET_PTR(args, 2, jmethodID);
			jvalue values[256];
			env->PushLocalFrame(128);
			parseValues(env, values, args[3], args[4]);			

			jobject ref = env->NewObjectA(cls, methodID, values);
			RETURN(JavaObject::wrap(env, ref, isolate));
			env->PopLocalFrame(NULL);
		}

		// invoke(vm, obj, methodID, types, args, retType, isStatic)
		void invoke(const FunctionCallbackInfo<Value>& args) {
			Isolate* isolate = Isolate::GetCurrent();
			EscapableHandleScope handle_scope(isolate);
			JavaVM* jvm = GET_JVM(args);

			JNIEnv* env = GET_ENV(jvm);
			jobject obj = UNWRAP(args[1], jobject); // a global reference
			jmethodID methodID = GET_PTR(args, 2, jmethodID);
			jvalue values[256];
			env->PushLocalFrame(128);
			parseValues(env,values, args[3], args[4]);

			Local<Value> ret;
			if(args[6]->BooleanValue()) { // is static
				jclass cls = (jclass) obj;
			
				switch(**String::Utf8Value(args[5])) {
				case 'V':
					env->CallStaticVoidMethodA(cls, methodID, values);
					break;
				case 'Z':
					ret = Boolean::New(isolate, env->CallStaticBooleanMethodA(cls, methodID, values));
					break;				
				case 'B':
					ret = Integer::New(isolate, env->CallStaticByteMethodA(cls, methodID, values));
					break;
				case 'S':
					ret = Integer::New(isolate, env->CallStaticShortMethodA(cls, methodID, values));
					break;
				case 'C':
					ret = Integer::NewFromUnsigned(isolate, env->CallStaticCharMethodA(cls, methodID, values));
					break;
				case 'I':
					ret = Integer::New(isolate, env->CallStaticIntMethodA(cls, methodID, values));
					break;
				case 'F':
					ret = Number::New(isolate, env->CallStaticFloatMethodA(cls, methodID, values));
					break;
				case 'D':
					ret = Number::New(isolate, env->CallStaticDoubleMethodA(cls, methodID, values));
					break;
				case 'J':
					ret = Number::New(isolate, env->CallStaticLongMethodA(cls, methodID, values));
					break;
				case '$':
					ret = cast(env, isolate, (jstring) env->CallStaticObjectMethodA(cls, methodID, values));
					break;
				case 'L':
					ret = JavaObject::wrap(env, env->CallStaticObjectMethodA(cls, methodID, values), isolate);
					break;
				}
			} else {
				switch(**String::Utf8Value(args[5])) {
				case 'V':
					env->CallVoidMethodA(obj, methodID, values);
					break;
				case 'Z':
					ret = Boolean::New(isolate, env->CallBooleanMethodA(obj, methodID, values));
					break;				
				case 'B':
					ret = Integer::New(isolate, env->CallByteMethodA(obj, methodID, values));
					break;
				case 'S':
					ret = Integer::New(isolate, env->CallShortMethodA(obj, methodID, values));
					break;
				case 'C':
					ret = Integer::NewFromUnsigned(isolate, env->CallCharMethodA(obj, methodID, values));
					break;
				case 'I':
					ret = Integer::New(isolate, env->CallIntMethodA(obj, methodID, values));
					break;
				case 'F':
					ret = Number::New(isolate, env->CallFloatMethodA(obj, methodID, values));
					break;
				case 'D':
					ret = Number::New(isolate, env->CallDoubleMethodA(obj, methodID, values));
					break;
				case 'J':
					ret = Number::New(isolate, env->CallLongMethodA(obj, methodID, values));
					break;
				case '$':
					ret = cast(env, isolate, (jstring) env->CallObjectMethodA(obj, methodID, values));
					break;
				case 'L':
					ret = JavaObject::wrap(env, env->CallObjectMethodA(obj, methodID, values), isolate);
					break;
				}
			}
			RETURN(ret);
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
		REGISTER(getClass);
#undef REGISTER
	}
}

NODE_MODULE(java, java::init)
