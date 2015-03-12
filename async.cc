/**
* 此文件实现了异步调用相关的libuv逻辑
*/

#include<uv.h>
#include "async.h"

using java::async::Task;

static void call(uv_work_t * work) {
    // printf("isolate is: %x", v8::Isolate::GetCurrent());
    Task *task = (Task *) work->data;
    task->execute();
}

static void after_call(uv_work_t *work, int status) {
    Task *task = (Task *) work->data;
    task->onFinish();
    delete task;
    delete work;
}

void java::async::Task::enqueue() {
    static uv_loop_t *loop = uv_default_loop();
    uv_work_t *work = new uv_work_t;
    work->data = this;
    uv_queue_work(loop, work, call, after_call);
}
