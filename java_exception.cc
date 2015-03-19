#include "java.h"
#include<string.h>

const void java::getJavaException(JNIEnv * env, jchar * &buf, int & len) {
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

    len = nameLen + msgLen + 3;
    if (!buf) {
        buf = new jchar[len];
    }

    env->GetStringRegion(clsName, 0, nameLen, buf);
    buf[nameLen] = buf[nameLen + 2] = ' ';
    buf[nameLen + 1] = ':';
    env->GetStringRegion(message, 0, msgLen, buf + nameLen + 3);

    env->PopLocalFrame(NULL);
}
