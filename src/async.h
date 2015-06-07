#ifndef    NODE_JAVA_ASYNC_H_
#define    NODE_JAVA_ASYNC_H_

#include<uv.h>
#include "java.h"

namespace java {
    void getJavaException(JNIEnv * env, jchar * &buf, size_t & len);

    namespace async {
        using namespace v8;

        void cb(uv_work_t * t);

        void cb_after(uv_work_t *t, int status);


        class Task {
        private:
            bool finalized; // whether finalized is called
            Isolate *isolate; // isolate can only be used in main thread.
            JavaVM *vm;
            JNIEnv *env; // JNI env in main thread. in running thread, obtain env with `JavaVM::AttachCurrentThread`

            uv_work_t work;
            Persistent <Promise::Resolver> resolver; // resolver associated with the promise returned
            union {
                jvalue value; // the value resolved
                jchar *msg; // the error message rejected
            };
            char resolved_type; // type of value resolved, or `'e'` if rejected
            size_t msg_len; // length of message rejected

            inline void onFinish();

        protected:
            virtual void finalize(JNIEnv *env);

            virtual void run(JNIEnv *env) = 0; // called by execute

            inline void reject(JNIEnv *env) {
                resolved_type = 'e';
                msg_len = 0;
                getJavaException(env, msg, msg_len);
            }

            inline void reject(const char *msg_, size_t msg_len_) {
                resolved_type = 'e';
                msg_len = msg_len_;
                msg = new jchar[msg_len_];
                for (size_t i = 0; i < msg_len_; i++)
                    msg[i] = msg_[i];
            }


            inline void resolve(const char type, jvalue val) {
                resolved_type = type;
                value = val;
            }

        public:

            inline Task(JavaVM *vm, JNIEnv *env, Isolate *isolate) :
                    finalized(false), isolate(isolate), vm(vm), env(env),
                    resolver(isolate, Promise::Resolver::New(isolate)) {
            }

            inline Local <Promise> promise() {
                static uv_loop_t *loop = uv_default_loop();
                work.data = this;
                uv_queue_work(loop, &work, cb, cb_after);
                return Local<Promise::Resolver>::New(isolate, resolver)->GetPromise();
            }

            friend void cb(uv_work_t *);

            friend void cb_after(uv_work_t *, int);
        };

        /**
        * runMainAsync的实现
        */
        class MainTask : public Task {
        private:
            jclass cls;
            jmethodID main;
            jobjectArray args;
        public:
            inline MainTask(JavaVM *vm, JNIEnv *env, jclass cls, jmethodID main, jobjectArray args, Isolate *isolate)
                    : Task(vm, env, isolate), cls(static_cast<jclass>(env->NewGlobalRef(cls))),
                      main(main), args(static_cast<jobjectArray>(env->NewGlobalRef(args))) {
            }

            // overrides pure abstract method async::Task::run(JNIEnv *)
            void run(JNIEnv *env);

            void finalize(JNIEnv *env);

        }; // End class MainTask


        class InvokeTask : public async::Task {
        public:
            jobject obj;
            JavaMethod *method;
            jvalue values[256];
            global_refs refs;

            inline InvokeTask(Isolate *isolate, JavaObject *handle, JavaMethod *method) :
                    Task(handle->jvm, handle->env, isolate),
                    obj(handle->env->NewGlobalRef(handle->_obj)),
                    method(method),
                    refs() {
                refs.push(obj);
            }

            void run(JNIEnv *env);

            void finalize(JNIEnv *env);
        };
    }
}

#endif
