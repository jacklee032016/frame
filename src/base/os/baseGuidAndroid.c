/* 
 *
 */

#include <baseGuid.h>
#include <baseLog.h>
#include <baseString.h>

#include <jni.h>

extern JavaVM *bjni_jvm;

static bbool_t attach_jvm(JNIEnv **jni_env)
{
    if ((*bjni_jvm)->GetEnv(bjni_jvm, (void **)jni_env,
                               JNI_VERSION_1_4) < 0)
    {
        if ((*bjni_jvm)->AttachCurrentThread(bjni_jvm, jni_env, NULL) < 0)
        {
            jni_env = NULL;
            return BASE_FALSE;
        }
        return BASE_TRUE;
    }
    
    return BASE_FALSE;
}

#define detach_jvm(attached) \
    if (attached) \
        (*bjni_jvm)->DetachCurrentThread(bjni_jvm);


const unsigned BASE_GUID_STRING_LENGTH=36;

unsigned bGUID_STRING_LENGTH()
{
    return BASE_GUID_STRING_LENGTH;
}

bstr_t* bgenerate_unique_string(bstr_t *str)
{
    jclass uuid_class;
    jmethodID get_uuid_method;
    jmethodID to_string_method;
    JNIEnv *jni_env = 0;
    jobject javaUuid;
    jstring uuid_string;
    const char *native_string;
    bstr_t native_str;

    bbool_t attached = attach_jvm(&jni_env);
    if (!jni_env)
        goto on_error;

    uuid_class = (*jni_env)->FindClass(jni_env, "java/util/UUID");

    if (uuid_class == 0)
        goto on_error;

    get_uuid_method = (*jni_env)->GetStaticMethodID(jni_env, uuid_class,
                      "randomUUID",
                      "()Ljava/util/UUID;");
    if (get_uuid_method == 0)
        goto on_error;

    javaUuid = (*jni_env)->CallStaticObjectMethod(jni_env, uuid_class, 
    						  get_uuid_method);
    if (javaUuid == 0)
        goto on_error;

    to_string_method = (*jni_env)->GetMethodID(jni_env, uuid_class,
    						"toString",
    						"()Ljava/lang/String;");
    if (to_string_method == 0)
        goto on_error;

    uuid_string = (*jni_env)->CallObjectMethod(jni_env, javaUuid,
    					       to_string_method);
    if (uuid_string == 0)
        goto on_error;

    native_string = (*jni_env)->GetStringUTFChars(jni_env, uuid_string,
    						  JNI_FALSE);
    if (native_string == 0)
        goto on_error;

    native_str.ptr = (char *)native_string;
    native_str.slen = bansi_strlen(native_string);
    bstrncpy(str, &native_str, BASE_GUID_STRING_LENGTH);

    (*jni_env)->ReleaseStringUTFChars(jni_env, uuid_string, native_string);
    (*jni_env)->DeleteLocalRef(jni_env, javaUuid);
    (*jni_env)->DeleteLocalRef(jni_env, uuid_class);
    (*jni_env)->DeleteLocalRef(jni_env, uuid_string);
    detach_jvm(attached);

    return str;

on_error:
    BASE_CRIT("Error generating UUID");
    detach_jvm(attached);
    return NULL;
}
