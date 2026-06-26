#include <jni.h>
#include <utility>
#include "AudioGraph.h"
#include "instruments/SampleDrum.h"
#include "outputs/OboeOutput.h"
#include "PianoBenchmark.h"

#define GRAPH(ptr) reinterpret_cast<AudioGraph*>(ptr)

extern "C" {

JNIEXPORT jlong JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_create(JNIEnv *, jobject) {
    return reinterpret_cast<jlong>(new AudioGraph());
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_start(JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->start();
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_stop(JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->stop();
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_destroy(JNIEnv *, jobject, jlong ptr) {
    delete GRAPH(ptr);
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_noteOn(
        JNIEnv *, jobject, jlong ptr, jint channel, jint note, jint velocity) {
    GRAPH(ptr)->noteOn(channel, note, velocity);
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_noteOff(
        JNIEnv *, jobject, jlong ptr, jint channel, jint note) {
    GRAPH(ptr)->noteOff(channel, note);
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_controlChange(
        JNIEnv *, jobject, jlong ptr, jint channel, jint cc, jint value) {
    GRAPH(ptr)->controlChange(channel, cc, value);
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_setInstrument(
        JNIEnv *env, jobject, jlong ptr, jint channel, jstring name) {
    const char *cname = env->GetStringUTFChars(name, nullptr);
    GRAPH(ptr)->setInstrument(channel, cname);
    env->ReleaseStringUTFChars(name, cname);
}


JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_setDrumBackend(
        JNIEnv *, jobject, jlong ptr, jint backendId) {
    static const char *ids[] = {"noise_drum", "fm_drum", "sample_drum"};
    if (backendId >= 0 && backendId < 3)
        GRAPH(ptr)->setInstrument(9, ids[backendId]);
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loadSample(
        JNIEnv *env, jobject, jlong ptr, jstring jname, jfloatArray jdata) {
    const char *name = env->GetStringUTFChars(jname, nullptr);
    int id = SampleDrum::nameToSampleId(name);
    env->ReleaseStringUTFChars(jname, name);
    if (id < 0) return;

    int length = env->GetArrayLength(jdata);
    jfloat *data = env->GetFloatArrayElements(jdata, nullptr);
    GRAPH(ptr)->loadDrumSample(id, data, length);
    env->ReleaseFloatArrayElements(jdata, data, JNI_ABORT);
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_setOutputPort(
        JNIEnv *env, jobject, jlong ptr, jobject jDevice, jobject jCallback) {
    GRAPH(ptr)->openMidiDevice(env, jDevice, jCallback);
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_clearOutputPort(
        JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->closeMidiDevice();
}

JNIEXPORT jstring JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_benchmarkPianos(JNIEnv *env, jobject) {
    std::string result = runPianoBenchmark();
    return env->NewStringUTF(result.c_str());
}


JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopStartRecord(
        JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->loopStartRecord();
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopStopRecord(
        JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->loopStopRecord();
}

JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopClear(
        JNIEnv *, jobject, jlong ptr) {
    GRAPH(ptr)->loopClear();
}

JNIEXPORT jint JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_loopState(
        JNIEnv *, jobject, jlong ptr) {
    return static_cast<jint>(GRAPH(ptr)->loopState());
}

} // extern "C"

extern "C"
JNIEXPORT void JNICALL
Java_org_gilbertxenodike_btmid_synth_NativeAudioEngine_setEngine(JNIEnv *env, jobject thiz, jlong ptr,
                                                        jint engine_id, jstring ip) {
    if (engine_id == 1) {
        GRAPH(ptr)->setEngine(1, "", 0);
    } else if (engine_id == 2) {
        const char *cip = env->GetStringUTFChars(ip, nullptr);
        std::string host(cip);
        env->ReleaseStringUTFChars(ip, cip);
        GRAPH(ptr)->setEngine(2, host, 5004);
    }
}