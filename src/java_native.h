#ifndef node_java_native_h_
#define node_java_native_h_

#include<jni.h>

namespace java {
    namespace native {
		JNIEXPORT void JNICALL voidCallback(JNIEnv *env, ...);

		JNIEXPORT jint JNICALL intCallback(JNIEnv *env, ...);

		JNIEXPORT jfloat JNICALL floatCallback(JNIEnv *env, ...);

		JNIEXPORT jdouble JNICALL doubleCallback(JNIEnv *env, ...);

		JNIEXPORT jlong JNICALL longCallback(JNIEnv *env, ...);

		JNIEXPORT jobject JNICALL objectCallback(JNIEnv *env, ...);

    }
}

#endif