#include "java.h"
#include "bindings.h"


namespace java {
    using namespace v8;
    bool verbose;
    Nan::Persistent <Object> bindings;
    Nan::Persistent <ObjectTemplate> javaObjectWrap;


    // init bindings
    void init(Handle < Object > exports) {
		  Nan::HandleScope _hscope;
        bindings.Reset(Local<Object>(exports));
        Local<ObjectTemplate> tmpl = Nan::New<ObjectTemplate>();
        tmpl->SetInternalFieldCount(1);

        javaObjectWrap.Reset(tmpl);
		// NanAssignPersistent(bindings, exports);
#define REGISTER(name)    Nan::SetMethod(exports, #name, vm::name);
#ifndef _WIN32
        REGISTER(link);
#endif
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
