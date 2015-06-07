#ifndef node_java_native_h_
#define node_java_native_h_

#include<jni.h>

namespace java {
    namespace native {
        extern void *callbacks[92];

        void init();
    }
}

#endif