#include "java.h"
#include<string.h>

void java::getJavaException(JNIEnv * env, jchar * &buf, size_t & len) {
    env->PushLocalFrame(0);
    jthrowable e = env->ExceptionOccurred();
    env->ExceptionClear();
    jclass cls = env->GetObjectClass(e);
    static jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Throwable"), "toString", "()Ljava/lang/String;");
    jstring message = (jstring) env->CallObjectMethod(e, toString);

    jsize msgLen = env->GetStringLength(message);
    if (len < msgLen) { // buf is not pre allocated, or buf insufficient
        buf = new jchar[msgLen];
    }
    len = msgLen;

    env->GetStringRegion(message, 0, msgLen, buf);
    env->PopLocalFrame(NULL);
}
