#include "java.h"
#include "bindings.h"


namespace java {
    using namespace v8;
    bool verbose;
    Persistent <Object> bindings;


    // init bindings
    void init(Handle < Object > exports) {
		NanScope();
		NanAssignPersistent(bindings, exports);
#define REGISTER(name)    NODE_SET_METHOD(exports, #name, vm::name);
        REGISTER(link);
        REGISTER(createVm);
        REGISTER(dispose);
        REGISTER(runMain);
        REGISTER(findClass);
        REGISTER(findMethod);
        REGISTER(findField);
        REGISTER(get);
        REGISTER(set);
        REGISTER(newInstance);
        REGISTER(invoke);
        REGISTER(invokeAsync);
        REGISTER(getClass);
        REGISTER(defineClass);
        REGISTER(getObjectArrayItem);
        REGISTER(setObjectArrayItem);
        REGISTER(getArrayLength);
        REGISTER(getPrimitiveArrayRegion);
        REGISTER(setPrimitiveArrayRegion);
#undef REGISTER
    }
}

#define INIT(module)    NODE_MODULE(module, module::init)
INIT(java)
#undef INIT
