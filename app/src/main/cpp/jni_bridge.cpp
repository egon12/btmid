#include <jni.h>
#include "NativeEngine.h"

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

} // extern "C"
