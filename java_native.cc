#include "java.h"

using namespace v8;

static jvalue callV8(JNIEnv * env, va_list & vl) {

}

void java::native::voidCallback(JNIEnv *env, ...) {
    va_list vl;
    va_start(vl, env);
    callV8(env, vl);
}