#ifndef    NODE_JAVA_ASYNC_H_
#define    NODE_JAVA_ASYNC_H_

#include<jni.h>
#include<v8.h>

namespace java {
    void getJavaException(JNIEnv * env, jchar * &buf, int & len);

    namespace async {
        using namespace v8;

        class Runner;

        class Task {
        private:
            bool finalized; // whether finalized is called
            Isolate *isolate; // isolate can only be used in main thread.
            JNIEnv *env; // JNI env in main thread. in running thread, obtain env with `JavaVM::AttachCurrentThread`
            void execute(); // called by thread pool
            void onFinish();

        protected:
            Persistent <Promise::Resolver> resolver; // resolver associated with the promise returned
            union {
                jvalue value; // the value resolved
                jchar *msg; // the error message rejected
            };
            char resolved_type; // type of value resolved, or `'e'` if rejected
            int msg_len; // length of message rejected
            virtual void finalize(JNIEnv *env) {
            }

            virtual void run(JNIEnv *env) = 0; // called by execute

            inline void reject(JNIEnv *env) {
                resolved_type = 'e';
                msg = NULL;
                getJavaException(env, msg, msg_len);
            }

            inline void reject(const char *msg_, int msg_len_) {
                resolved_type = 'e';
                msg_len = msg_len_;
                msg = new jchar[msg_len_];
                for (int i = 0; i < msg_len_; i++)
                    msg[i] = msg_[i];
            }


            inline void resolve(const char type, jvalue val) {
                resolved_type = type;
                value = val;
            }

        public:
            JavaVM *vm;

            inline Task(JavaVM *vm, JNIEnv *env, Isolate *isolate) :
                    finalized(false), vm(vm), env(env), isolate(isolate),
                    resolver(isolate, Promise::Resolver::New(isolate)) {
            }


            void enqueue();

            inline Local <Promise> promise() {
                return Local<Promise::Resolver>::New(isolate, resolver)->GetPromise();
            }


            friend class Runner;

        };
    }
}

#endif
