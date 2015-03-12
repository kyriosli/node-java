#include "java.h"
#include<string.h>

const jchar *java::getJavaException(JNIEnv *env, int *len) {
    env->PushLocalFrame(0);
    jthrowable e = env->ExceptionOccurred();
    env->ExceptionClear();
    jclass cls = env->GetObjectClass(e);
    static jmethodID getName = env->GetMethodID(env->GetObjectClass(cls), "getName", "()Ljava/lang/String;");
    jstring clsName = (jstring) env->CallObjectMethod(cls, getName);

    jmethodID getMessage = env->GetMethodID(cls, "getMessage", "()Ljava/lang/String;");
    jstring message = (jstring) env->CallObjectMethod(e, getMessage);

    jsize nameLen = env->GetStringLength(clsName),
            msgLen = env->GetStringLength(message);

    jchar *buf = new jchar[nameLen + msgLen + 3];
    const jchar *chars = env->GetStringCritical(clsName, NULL);
    memcpy(buf, chars, nameLen << 1);
    buf[nameLen] = buf[nameLen + 2] = ' ';
    buf[nameLen + 1] = ':';
    env->ReleaseStringCritical(clsName, chars);

    chars = env->GetStringCritical(message, NULL);
    memcpy(buf + nameLen + 3, chars, msgLen << 1);
    env->ReleaseStringCritical(message, chars);

    if (len) *len = nameLen + msgLen + 3;
    env->PopLocalFrame(NULL);
    return buf;
}
