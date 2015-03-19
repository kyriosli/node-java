#ifndef    NODE_JAVA_ASYNC_H_
#define    NODE_JAVA_ASYNC_H_

#include<jni.h>
#include<v8.h>

namespace java {
    namespace async {
        using namespace v8;

        class Task {
        private:
            bool finalized;
            JNIEnv *env;
        protected:
            Persistent <Promise::Resolver> resolver;
            union {
                jvalue value;
                const jchar *msg;
            };
            char resolved_type;
            int msg_len;
        public:
            JavaVM *vm;

            inline Task(JavaVM *vm, JNIEnv *env, Isolate *isolate) :
                    finalized(false), vm(vm), env(env), resolver(isolate, Promise::Resolver::New(isolate)) {

            }

            void execute();

            virtual void run(JNIEnv *env) = 0;

            virtual void finalize(JNIEnv *env) {
            }

            void enqueue();

            void onFinish();

            inline Local <Promise> promise(Isolate *isolate) {
                return Local<Promise::Resolver>::New(isolate, resolver)->GetPromise();
            }

            inline void reject(const jchar *msg_, int msg_len_) {
                resolved_type = 'e';
                msg = msg_;
                msg_len = msg_len_;
            }

            inline void resolve(const char type, jvalue val) {
                resolved_type = type;
                value = val;
            }


        };

        Task &newTask(JavaVM *, Isolate *);

    }
}

#endif
