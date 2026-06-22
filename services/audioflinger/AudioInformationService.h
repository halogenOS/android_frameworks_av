/*
 * Copyright (C) 2026 The halogenOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <android-base/thread_annotations.h>
#include <custom/media/audio_information/AudioPathInfo.h>
#include <custom/media/audio_information/BnAudioInformation.h>
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>

namespace android {

class AudioFlinger;
class IAfPlaybackThread;
class IAfMmapThread;
class IAfThreadBase;

/**
 * Read-only diagnostic binder service that reports the active audio output paths
 * (mixer/hardware stages, flags and output-chain effects) as observed inside
 * audioserver. It is hosted by audioserver next to AudioFlinger and reads
 * AudioFlinger state in-process under AudioFlinger::mutex(); the returned data is
 * a snapshot that may lag the live state by up to one thread loop.
 */
class AudioInformationService : public ::custom::media::audio_information::BnAudioInformation {
public:
    static constexpr const char* kServiceName = "custom.media.audio_information";

    explicit AudioInformationService(const sp<AudioFlinger>& audioFlinger);

    // BnAudioInformation
    ::android::binder::Status listActiveAudioPaths(
            std::vector<::custom::media::audio_information::AudioPathInfo>* _aidl_return) override;
    ::android::binder::Status getAudioPathInfo(
            int32_t ioHandle,
            ::custom::media::audio_information::AudioPathInfo* _aidl_return) override;

private:
    // Fills out a parcelable for a single playback thread. Caller holds AudioFlinger::mutex().
    void describePlaybackThread_l(
            IAfPlaybackThread* thread,
            ::custom::media::audio_information::AudioPathInfo* out) const;
    // Fills out a parcelable for a single MMAP thread. Caller holds AudioFlinger::mutex().
    void describeMmapThread_l(
            IAfMmapThread* thread,
            ::custom::media::audio_information::AudioPathInfo* out) const;
    // Computes the bit-perfect verdict (out->bitPerfect / activeTrackCount / bitPerfectReasons)
    // for a playback thread. Caller holds both AudioFlinger::mutex() and the thread's mutex.
    void describeBitPerfect_l(
            IAfPlaybackThread* thread,
            ::custom::media::audio_information::AudioPathInfo* out) const NO_THREAD_SAFETY_ANALYSIS;
    // Collects the output-chain effects of a thread into out->effects. Caller
    // holds both AudioFlinger::mutex() and the thread's mutex; the latter cannot
    // be expressed for a parameter, so analysis is suppressed.
    void collectEffects_l(
            IAfThreadBase* thread,
            ::custom::media::audio_information::AudioPathInfo* out) const
            NO_THREAD_SAFETY_ANALYSIS;

    const sp<AudioFlinger> mAudioFlinger;
};

}  // namespace android
