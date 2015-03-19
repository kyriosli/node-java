/**
* 此文件实现了异步调用相关的java逻辑
*/

#include "java.h"
#include "async.h"

using namespace v8;

void java::async::Task::onFinish() {
    Isolate *isolate = Isolate::GetCurrent();
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
}

void java::async::Task::execute() {
    JNIEnv *env;
    jint errno = vm->AttachCurrentThread((void **) &env, NULL);
    if (errno) {
        char buf[64];
        int count = sprintf(buf, "`AttachCurrentThread' failed with errno %d", errno);
        jchar *msg = new jchar[count];
        printw(msg, buf, count);
        reject(msg, count);
        return;
    }
    run(env); // this is virtual
    finalize(env);
    finalized = true;
    vm->DetachCurrentThread();
}
