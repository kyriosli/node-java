#ifndef node_java_native_h_
#define node_java_native_h_

#include<jni.h>

namespace java {
    namespace native {
        void voidCallback(JNIEnv *env, ...);

        jint intCallback(JNIEnv *env, ...);

        jfloat floatCallback(JNIEnv *env, ...);

        jdouble doubleCallback(JNIEnv *env, ...);

        jlong longCallback(JNIEnv *env, ...);

        jobject objectCallback(JNIEnv *env, ...);
    }
}

#endif