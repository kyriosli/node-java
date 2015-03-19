#include "java.h"
#include<string.h>

void java::getJavaException(JNIEnv * env, jchar * &buf, int & len) {
    env->PushLocalFrame(0);
    jthrowable e = env->ExceptionOccurred();
    env->ExceptionClear();
    jclass cls = env->GetObjectClass(e);
    static jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Throwable"), "toString", "()Ljava/lang/String;");
    jstring message = (jstring) env->CallObjectMethod(e, toString);

    len = env->GetStringLength(message);

    if (!buf) {
        buf = new jchar[len];
    }

    env->GetStringRegion(message, 0, len, buf);
    env->PopLocalFrame(NULL);
}
