#ifndef	NODE_JAVA_ASYNC_H_
#define NODE_JAVA_ASYNC_H_
#include<jni.h>
#include<v8.h>

namespace java {
	namespace async {
		using namespace v8;
		typedef union {
			jvalue value;
			const jchar* msg;
		} resolve_info;

		class Task {
		protected:
			Persistent<Promise::Resolver> resolver;
			resolve_info resolved;
			char resolved_type;
			int msg_len;
		public:
			JavaVM* vm;

			inline Task(JavaVM* vm, Isolate* isolate) :
				vm(vm), resolver(isolate, Promise::Resolver::New(isolate)) {
				
			}
			void execute();
			virtual void run(JNIEnv* env) = 0; 
			void enqueue ();
			void resolve();
		
			inline Local<Promise> promise(Isolate* isolate) {
				return Local<Promise::Resolver>::New(isolate, resolver)->GetPromise();
			}

			inline void reject(const jchar* msg, int msg_len_) {
				resolved_type = 'e';
				resolved.msg = msg;
				msg_len = msg_len_;
			}

		};

		Task& newTask(JavaVM*, Isolate*);

	}
}

#endif
