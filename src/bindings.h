#ifndef java_bindings_h_
#define java_bindings_h_

#include "java.h"


namespace java {
    namespace vm {

        NAN_METHOD(createVm);

        // runMain(vm, cls, args, async)
        NAN_METHOD(runMain);

        // findClass(vm, name, classCache)
        NAN_METHOD(findClass);

        // getClass(obj, cache, self)
        NAN_METHOD(getClass);

        // findField(cls, name, type, isStatic)
        NAN_METHOD(findField);

        // get(handle, field)
        NAN_METHOD(get);

        // set(handle, field, value)
        NAN_METHOD(set);


        // findMethod(cls, signature, isStatic)
        NAN_METHOD(findMethod);


        // newInstance(cls, method, args)
        NAN_METHOD(newInstance);


        // invokeAsync(obj, method, args)
        NAN_METHOD(invokeAsync);

        // invoke(obj, method, args)
        NAN_METHOD(invoke);

        NAN_METHOD(dispose);

        // link(path, verbose)
        NAN_METHOD(link);

        // defineClass(vm, name, buffer, natives)
        NAN_METHOD(defineClass);

        NAN_METHOD(getObjectArrayItem);

        NAN_METHOD(setObjectArrayItem);

        NAN_METHOD(getArrayLength);

        NAN_METHOD(getPrimitiveArrayRegion);

        NAN_METHOD(setPrimitiveArrayRegion);

    }
}

#endif
