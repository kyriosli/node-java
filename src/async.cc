/**
* 此文件实现了异步调用相关的libuv逻辑
*/

#include "async.h"

// #include "async.h"

inline void java::async::Task::onFinish() {
    HandleScope handle_scope(isolate);

    Local <Promise::Resolver> res = Local<Promise::Resolver>::New(isolate, resolver);

    if (resolved_type == 'e') {
        // reject
        Local <String> jsmsg = String::NewFromTwoByte(isolate, msg, String::kNormalString, msg_len);
        delete[] msg;
        res->Reject(Exception::Error(jsmsg));
    } else if (resolved_type == 'V') {
        res->Resolve(Undefined(isolate));
    } else if ((resolved_type == '$' || resolved_type == 'L') && !value.l) {
        res->Resolve(Null(isolate));
    } else {
        res->Resolve(convert(resolved_type, isolate, vm, env, value));
    }

    if (!finalized) {
        finalize(env);
    }
    resolver.Reset();
}

void java::async::cb(uv_work_t * work) { // called in thread pool
    Task &task = *(Task *) work->data;
    JNIEnv *env;
    jint err = task.vm->AttachCurrentThread((void **) &env, NULL);
    if (err) {
        char buf[64];
        int count = sprintf(buf, "`AttachCurrentThread' failed with errno %d", err);
        task.reject(buf, count);
        return;
    }
    task.run(env); // this is virtual
    task.finalize(env);
    task.finalized = true;
    task.vm->DetachCurrentThread();
}

void java::async::cb_after(uv_work_t *work, int status) { // called in main message loop
    Task *task = (Task *) work->data;
    task->onFinish();
    delete task;
}


// default finalizer
void java::async::Task::finalize(JNIEnv * env) {
}


// overrides pure abstract method async::Task::run(JNIEnv *)
void java::async::MainTask::run(JNIEnv * env) {
    jvalue jargs;
    jargs.l = args;
    env->CallStaticVoidMethodA(cls, main, &jargs);
    // check exception

    if (env->ExceptionCheck()) {
        reject(env);
    } else {
        resolve('V', jvalue());
    }
}

void java::async::MainTask::finalize(JNIEnv * env) {
    env->DeleteGlobalRef(cls);
    env->DeleteGlobalRef(args);
}

void java::async::InvokeTask::run(JNIEnv * env) {
    env->PushLocalFrame(128);
    jvalue ret;
    java::invoke(env, obj, method, values, ret);


    if (env->ExceptionCheck()) {
        reject(env);
    } else {
        resolve(method->retType, ret);
    }

    env->PopLocalFrame(NULL);

}

void java::async::InvokeTask::finalize(JNIEnv * env) {
    refs.free(env);
}
