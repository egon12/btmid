#include <jni.h>
#include "NativeEngine.h"
#include "instruments/SampleDrum.h"

#define ENGINE(ptr) reinterpret_cast<NativeEngine*>(ptr)

extern "C" {

JNIEXPORT jlong JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_create(JNIEnv*, jobject) {
    return reinterpret_cast<jlong>(new NativeEngine());
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_start(JNIEnv*, jobject, jlong ptr) {
    ENGINE(ptr)->start();
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_stop(JNIEnv*, jobject, jlong ptr) {
    ENGINE(ptr)->stop();
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_destroy(JNIEnv*, jobject, jlong ptr) {
    delete ENGINE(ptr);
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_noteOn(
        JNIEnv*, jobject, jlong ptr, jint channel, jint note, jint velocity) {
    ENGINE(ptr)->noteOn(channel, note, velocity);
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_noteOff(
        JNIEnv*, jobject, jlong ptr, jint channel, jint note) {
    ENGINE(ptr)->noteOff(channel, note);
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_setDrumBackend(
        JNIEnv*, jobject, jlong ptr, jint backendId) {
    ENGINE(ptr)->setDrumBackend(backendId);
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_loadSample(
        JNIEnv* env, jobject, jlong ptr, jstring jname, jfloatArray jdata) {
    const char* name = env->GetStringUTFChars(jname, nullptr);
    int id = SampleDrum::nameToSampleId(name);
    env->ReleaseStringUTFChars(jname, name);
    if (id < 0) return;

    int     length = env->GetArrayLength(jdata);
    jfloat* data   = env->GetFloatArrayElements(jdata, nullptr);
    ENGINE(ptr)->loadSample(id, data, length);
    env->ReleaseFloatArrayElements(jdata, data, JNI_ABORT);
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_setOutputPort(
        JNIEnv* env, jobject, jlong ptr, jobject jDevice, jobject jCallback) {
    ENGINE(ptr)->setOutputPort(env, jDevice, jCallback);
}

JNIEXPORT void JNICALL
Java_org_egon12_btmid_synth_NativeAudioEngine_clearOutputPort(
        JNIEnv*, jobject, jlong ptr) {
    ENGINE(ptr)->clearOutputPort();
}

} // extern "C"
