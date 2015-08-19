#include<jni.h>
#include<string.h>

namespace java {
    void getJavaException(JNIEnv * env, jchar * &buf, size_t & len) {
        env->PushLocalFrame(0);
        jthrowable e = env->ExceptionOccurred();
        env->ExceptionClear();

        static jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Throwable"), "toString", "()Ljava/lang/String;");
        jstring message = (jstring) env->CallObjectMethod(e, toString);

        jsize msgLen = env->GetStringLength(message);
        if (len < (size_t) msgLen) { // buf is not pre allocated, or buf insufficient
            buf = new jchar[msgLen];
        }
        len = msgLen;

        env->GetStringRegion(message, 0, msgLen, buf);
        env->PopLocalFrame(NULL);
    }
}