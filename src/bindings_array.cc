#include "java.h"
#include<node_buffer.h>

namespace java {


    namespace vm {
        // getObjectArrayItem(handle, index)
        NAN_METHOD(getObjectArrayItem) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject *);
            int idx = args[1]->Int32Value();

            JNIEnv *env = handle->env;
            jobjectArray arr = static_cast<jobjectArray>(handle->_obj);
            jsize maxLen = env->GetArrayLength(arr);
            if (idx < 0 || idx >= maxLen) {
                return NanThrowError("array index out of range");
            }
            jobject ret = env->GetObjectArrayElement(arr, idx);
            if (ret) {
                RETURN(JavaObject::wrap(handle->jvm, env, ret, isolate));
            } else {
                args.GetReturnValue().SetNull();
            }

        }

        // getPrimitiveArrayRegion(handle, start, end, type)
        void getPrimitiveArrayRegion(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject *);


            JNIEnv *env = handle->env;
            jarray arr = static_cast<jarray>(handle->_obj);
            jsize maxLen = env->GetArrayLength(arr);

            int start, end;

            if (args[1]->IsUndefined()) {
                start = 0;
                end = maxLen;
            } else {
                start = args[1]->Int32Value();
                end = args[2]->IsUndefined() ? maxLen : args[2]->Int32Value();
            }

            if (start < 0 || end < start || end > maxLen) {
                return NanThrowError("array index out of range");
                return;
            }

            uint32_t len = end - start, size = 0;

            void *buf;
            switch (args[3]->Int32Value()) {
                case 'Z':  // boolean
                    size = len * sizeof(jboolean);
                    env->GetBooleanArrayRegion(static_cast<jbooleanArray>(arr), start, len, static_cast<jboolean *>(buf = new char[size]));
                    break;
                case 'B': // byte
                    size = len * sizeof(jbyte);
                    env->GetByteArrayRegion(static_cast<jbyteArray>(arr), start, len, static_cast<jbyte *>(buf = new char[size]));
                    break;
                case 'S': // short
                    size = len * sizeof(jshort);
                    env->GetShortArrayRegion(static_cast<jshortArray>(arr), start, len, static_cast<jshort *>(buf = new char[size]));
                    break;
                case 'C': // char
                    size = len * sizeof(jchar);
                    env->GetCharArrayRegion(static_cast<jcharArray>(arr), start, len, static_cast<jchar *>(buf = new char[size]));
                    break;
                case 'I': // int
                    size = len * sizeof(jint);
                    env->GetIntArrayRegion(static_cast<jintArray>(arr), start, len, static_cast<jint *>(buf = new char[size]));
                    break;
                case 'F': // float
                    size = len * sizeof(jfloat);
                    env->GetFloatArrayRegion(static_cast<jfloatArray>(arr), start, len, static_cast<jfloat *>(buf = new char[size]));
                    break;
                case 'J': // long
                    size = len * sizeof(jlong);
                    env->GetLongArrayRegion(static_cast<jlongArray>(arr), start, len, static_cast<jlong *>(buf = new char[size]));
                    break;
                case 'D': // double
                    size = len * sizeof(jdouble);
                    env->GetDoubleArrayRegion(static_cast<jdoubleArray>(arr), start, len, static_cast<jdouble *>(buf = new char[size]));
                    break;
            }
//            fprintf(stderr, "new buffer %s %d type:%d\n", buf, size, args[3]->Int32Value());
            RETURN(node::Buffer::New(isolate, static_cast<char *>(buf), size));
            delete[] buf;

        }

        // setObjectArrayItem(handle, index, value)
        void setObjectArrayItem(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject *);
            int idx = args[1]->Int32Value();
            JNIEnv *env = handle->env;
            jobjectArray arr = static_cast<jobjectArray>(handle->_obj);
            jsize maxLen = env->GetArrayLength(arr);
            if (idx < 0 || idx >= maxLen) {
                return NanThrowError("array index out of range");
            }

            Local <Value> val = args[2];
            jobject obj;
            if (val->IsString()) {
                obj = cast(env, val);
            } else if (val->IsNull()) {
                obj = NULL;
            } else {
                JavaObject *handle = GET_PTR(args, 2, JavaObject *);
                obj = handle->_obj;
            }
            env->SetObjectArrayElement(arr, idx, obj);
            if (obj) { // check exception
                if (env->ExceptionCheck()) {
                    return ThrowJavaException(env, isolate);
                }
            }
        }

        // setPrimitiveArrayRegion(handle, start, end, type, buffer)
        void setPrimitiveArrayRegion(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject *);


            JNIEnv *env = handle->env;
            jarray arr = static_cast<jarray>(handle->_obj);
            jsize maxLen = env->GetArrayLength(arr);

            int start, end;

            if (args[1]->IsUndefined()) {
                start = 0;
                end = maxLen;
            } else {
                start = args[1]->Int32Value();
                end = args[2]->IsUndefined() ? maxLen : args[2]->Int32Value();
            }

            if (start < 0 || end < start || end > maxLen) {
                return NanThrowError("array index out of range");
            }

            size_t len = end - start;

            size_t bufLen = node::Buffer::Length(args[4]);
            void *buf = node::Buffer::Data(args[4]);

            switch (args[3]->Int32Value()) {
                case 'Z':  // boolean
                    if ((bufLen /= sizeof(jboolean)) < len) len = bufLen;
                    env->SetBooleanArrayRegion(static_cast<jbooleanArray>(arr), start, len, static_cast<jboolean *>(buf));
                    break;
                case 'B': // byte
                    if ((bufLen /= sizeof(jbyte)) < len) len = bufLen;
                    env->SetByteArrayRegion(static_cast<jbyteArray>(arr), start, len, static_cast<jbyte *>(buf));
                    break;
                case 'S': // short
                    if ((bufLen /= sizeof(jshort)) < len) len = bufLen;
                    env->SetShortArrayRegion(static_cast<jshortArray>(arr), start, len, static_cast<jshort *>(buf));
                    break;
                case 'C': // char
                    if ((bufLen /= sizeof(jchar)) < len) len = bufLen;
                    env->SetCharArrayRegion(static_cast<jcharArray>(arr), start, len, static_cast<jchar *>(buf));
                    break;
                case 'I': // int
                    if ((bufLen /= sizeof(jint)) < len) len = bufLen;
                    env->SetIntArrayRegion(static_cast<jintArray>(arr), start, len, static_cast<jint *>(buf));
                    break;
                case 'F': // float
                    if ((bufLen /= sizeof(jfloat)) < len) len = bufLen;
                    env->SetFloatArrayRegion(static_cast<jfloatArray>(arr), start, len, static_cast<jfloat *>(buf));
                    break;
                case 'J': // long
                    if ((bufLen /= sizeof(jlong)) < len) len = bufLen;
                    env->SetLongArrayRegion(static_cast<jlongArray>(arr), start, len, static_cast<jlong *>(buf));
                    break;
                case 'D': // double
                    if ((bufLen /= sizeof(jdouble)) < len) len = bufLen;
                    env->SetDoubleArrayRegion(static_cast<jdoubleArray>(arr), start, len, static_cast<jdouble *>(buf));
                    break;
            }
        }

        //getArrayLength(handle)
        void getArrayLength(const FunctionCallbackInfo <Value> &args) {
            Isolate *isolate = Isolate::GetCurrent();
            HandleScope handle_scope(isolate);
            JavaObject *handle = GET_PTR(args, 0, JavaObject *);

            JNIEnv *env = handle->env;
            jobjectArray arr = static_cast<jobjectArray>(handle->_obj);
            jsize maxLen = env->GetArrayLength(arr);

            RETURN(Integer::New(isolate, maxLen));
        }
    }
}
