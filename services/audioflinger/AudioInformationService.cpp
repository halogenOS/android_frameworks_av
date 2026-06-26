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

#define LOG_TAG "AudioInformationService"
//#define LOG_NDEBUG 0

#include "AudioInformationService.h"

#include "AudioFlinger.h"
#include "IAfEffect.h"
#include "IAfThread.h"
#include "IAfTrack.h"
#include "datapath/AudioStreamOut.h"

#include <audio_utils/mutex.h>
#include <media/AudioEffect.h>
#include <system/audio.h>
#include <system/audio_effect.h>
#include <utils/Log.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace android {

using ::custom::media::audio_information::AudioEffectInfo;
using ::custom::media::audio_information::AudioPathInfo;
using ::custom::media::audio_information::AudioSourceInfo;

// Path type ints, mirroring the frozen IAudioInformation contract.
namespace {
constexpr int32_t kPathTypeMixed = 0;
constexpr int32_t kPathTypeDirect = 1;
constexpr int32_t kPathTypeOffload = 2;
constexpr int32_t kPathTypeMmapExclusive = 3;
constexpr int32_t kPathTypeBitPerfect = 4;
constexpr int32_t kPathTypeSpatializer = 5;
constexpr int32_t kPathTypeUnknown = 6;

std::string uuidToString(const effect_uuid_t& uuid) {
    char str[EFFECT_STRING_LEN_MAX] = {};
    AudioEffect::guidToString(&uuid, str, sizeof(str));
    return std::string(str);
}
}  // namespace

AudioInformationService::AudioInformationService(const sp<AudioFlinger>& audioFlinger)
    : mAudioFlinger(audioFlinger) {}

binder::Status AudioInformationService::listActiveAudioPaths(
        std::vector<AudioPathInfo>* _aidl_return) {
    _aidl_return->clear();
    if (mAudioFlinger == nullptr) {
        return binder::Status::ok();
    }

    audio_utils::lock_guard _l(mAudioFlinger->mutex());
    for (const auto& [_, thread] : mAudioFlinger->mPlaybackThreads) {
        AudioPathInfo info;
        describePlaybackThread_l(thread.get(), &info);
        _aidl_return->push_back(std::move(info));
    }
    for (const auto& [_, thread] : mAudioFlinger->mMmapThreads) {
        // Only report MMAP playback paths; capture threads are out of scope.
        if (thread->type() != IAfThreadBase::MMAP_PLAYBACK) {
            continue;
        }
        AudioPathInfo info;
        describeMmapThread_l(thread.get(), &info);
        _aidl_return->push_back(std::move(info));
    }
    return binder::Status::ok();
}

binder::Status AudioInformationService::getAudioPathInfo(
        int32_t ioHandle, AudioPathInfo* _aidl_return) {
    *_aidl_return = AudioPathInfo();
    _aidl_return->ioHandle = ioHandle;
    _aidl_return->pathType = kPathTypeUnknown;
    if (mAudioFlinger == nullptr) {
        return binder::Status::ok();
    }

    const auto handle = static_cast<audio_io_handle_t>(ioHandle);
    audio_utils::lock_guard _l(mAudioFlinger->mutex());
    if (IAfPlaybackThread* const thread = mAudioFlinger->checkPlaybackThread_l(handle);
            thread != nullptr) {
        describePlaybackThread_l(thread, _aidl_return);
    } else if (IAfMmapThread* const mmapThread = mAudioFlinger->checkMmapThread_l(handle);
            mmapThread != nullptr && mmapThread->type() == IAfThreadBase::MMAP_PLAYBACK) {
        describeMmapThread_l(mmapThread, _aidl_return);
    }
    return binder::Status::ok();
}

void AudioInformationService::describePlaybackThread_l(
        IAfPlaybackThread* thread, AudioPathInfo* out) const {
    // Lock ordering is AudioFlinger > ThreadBase > EffectChain; the AudioFlinger
    // mutex is already held by the caller. Take the thread mutex to read the
    // output and effect chains coherently.
    audio_utils::lock_guard _tl(thread->mutex());

    out->ioHandle = thread->id();

    // Path classification follows the openOutput_l branch order: the thread
    // type_t is authoritative, with the output flag bitmask as corroboration.
    AudioStreamOut* const output = thread->getOutput_l();
    const audio_output_flags_t flags = output != nullptr ? output->flags : AUDIO_OUTPUT_FLAG_NONE;
    out->outputFlags = static_cast<int32_t>(flags);

    const bool hasMixer = thread->hasMixer();
    switch (thread->type()) {
    case IAfThreadBase::MIXER:
        out->pathType = kPathTypeMixed;
        break;
    case IAfThreadBase::DUPLICATING:
        // DuplicatingThread subclasses MixerThread (hasMixer() is true); it shares the
        // mixer pipeline, so it is a mixed path rather than UNKNOWN.
        out->pathType = kPathTypeMixed;
        break;
    case IAfThreadBase::DIRECT:
        // OffloadThread reports type OFFLOAD; a DIRECT type with the offload flag
        // is still effectively compressed passthrough.
        out->pathType = (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) != 0
                ? kPathTypeOffload : kPathTypeDirect;
        break;
    case IAfThreadBase::OFFLOAD:
        out->pathType = kPathTypeOffload;
        break;
    case IAfThreadBase::SPATIALIZER:
        out->pathType = kPathTypeSpatializer;
        break;
    case IAfThreadBase::BIT_PERFECT:
        out->pathType = kPathTypeBitPerfect;
        break;
    default:
        out->pathType = kPathTypeUnknown;
        break;
    }

    // Mixer stage: present only when the thread actually mixes (MIXER /
    // SPATIALIZER / BIT_PERFECT). For bypass paths these fields stay zeroed and
    // hasMixerStage is false.
    out->hasMixerStage = hasMixer;
    if (hasMixer) {
        out->mixSampleRate = static_cast<int32_t>(thread->sampleRate());
        out->mixFormat = static_cast<int32_t>(thread->mixFormat());
        out->mixChannelMask = static_cast<int32_t>(thread->mixerChannelMask());
        // The mixer's internal accumulation precision (typically PCM_FLOAT), distinct from the sink
        // format above. Read under the held thread mutex; mMixerBufferFormat is written only in
        // readOutputParameters_l() under mutex() and only read elsewhere, so it is stable here. Left
        // at 0 (AUDIO_FORMAT_INVALID) for bypass paths with no mixer buffer.
        out->mixInternalFormat = static_cast<int32_t>(thread->mixerBufferFormat());
    }
    out->hasFastMixer = thread->hasFastMixer();

    // Hardware/DAC stage: always present. sampleRate()/channelMask() report the
    // HAL-negotiated values and format() returns mHALFormat (the DAC-side format).
    out->hardwareSampleRate = static_cast<int32_t>(thread->sampleRate());
    out->hardwareFormat = static_cast<int32_t>(thread->format());
    out->hardwareChannelMask = static_cast<int32_t>(thread->channelMask());

    // Own sink device(s): the audio_port_handle_t of each sink in the thread's patch — the exact key
    // the client matches against AudioDeviceInfo.getId(). Empty when the thread has no patch yet
    // (mPatch.num_sinks == 0); the client then omits the output-device identity rather than guess.
    // Read under the held thread mutex above.
    out->sinkPortIds = thread->outDevicePortIds_l();

    // Active source stage: EVERY active external (client) track's PCM format/rate/channels — the only
    // place the real per-track source bit depth is observable (AudioPlaybackConfiguration cannot
    // expose it). A thread's "source" is all of its active external tracks, not one: emitting only the
    // first would be a banned pointer-order heuristic (getTracks_l() is pointer-address order, not
    // signal order). The active filter (isTrackActive_l && state()==ACTIVE) mirrors describeBitPerfect_l
    // below, so a paused/stopped client track is not reported as a live source.
    // Per-track resampling is the sound signal: the track's source rate vs the thread's device rate,
    // not a mix-rate vs hardware-rate comparison. With no active external track this stays empty.
    // getTracks_l()/isTrackActive_l() require the thread mutex, already held above.
    out->sources.clear();
    const uint32_t threadSampleRate = thread->sampleRate();
    for (const sp<IAfTrackBase>& tb : thread->getTracks_l()) {
        if (tb == nullptr || !tb->isExternalTrack()) {
            continue;
        }
        const sp<IAfTrack> track = tb->asIAfTrack();
        if (track == nullptr) {
            continue;
        }
        if (!thread->isTrackActive_l(track) || track->state() != IAfTrackBase::ACTIVE) {
            continue;
        }
        AudioSourceInfo source;
        source.sampleRate = static_cast<int32_t>(track->sampleRate());
        source.format = static_cast<int32_t>(track->format());
        source.channelMask = static_cast<int32_t>(track->channelMask());
        source.resampling = track->sampleRate() != threadSampleRate;
        // The track's NEGOTIATED output flags (effective per-track flags after AF negotiation),
        // a source-side property distinct from the thread flags. NOT the raw app request — that is
        // a discarded local in createTrack_l() and is unreadable here. Read per-track off the
        // sp<IAfTrack> already obtained above; never derived from the thread's flags.
        source.outputFlags = static_cast<int32_t>(track->getOutputFlags());
        out->sources.push_back(std::move(source));
    }

    // We already hold thread->mutex(); call the lock-held variant. The public latency() re-acquires
    // the (non-recursive) thread mutex and would deadlock/abort under our lock.
    out->latencyMs = static_cast<int32_t>(thread->latency_l());

    describeBitPerfect_l(thread, out);

    collectEffects_l(thread, out);
}

void AudioInformationService::describeBitPerfect_l(
        IAfPlaybackThread* thread, AudioPathInfo* out) const {
    // Bit-perfect verdict, replicating AudioFlinger's getTrackToStreamBitPerfectly_l()/mIsBitPerfect
    // logic (that member is guarded by a different lock and not readable here, so we recompute it):
    // a thread is delivering bit-exact samples only when it is a BIT_PERFECT thread carrying exactly
    // one active bit-perfect track with positive applied gain and every other active track muted.
    //
    // A normal MixerThread is NEVER bit-perfect: it float-accumulates and requantizes on output, and
    // its volume stage always multiplies (no unity-memcpy bypass). So neither unity volume nor
    // source-format == output-format implies bit-exact — we never claim either.
    //
    // All accessors below are read under the thread mutex held by the caller:
    //   isTrackActive_l(sp<IAfTrack>), track->state(), track->isBitPerfect(),
    //   track->getFinalVolume() (the complete combined applied gain — the only correct volume
    //   signal), track->sampleRate(), thread->sampleRate().
    const uint32_t deviceRate = thread->sampleRate();

    int32_t activeTrackCount = 0;
    int32_t qualifyingBitPerfectTracks = 0;  // active bit-perfect track with positive gain
    int32_t unmutedNonBitPerfectTracks = 0;  // any other active track that is NOT muted
    bool anyResampling = false;  // ≥1 active track whose source rate != device rate

    for (const sp<IAfTrackBase>& tb : thread->getTracks_l()) {
        if (tb == nullptr) {
            continue;
        }
        const sp<IAfTrack> track = tb->asIAfTrack();
        if (track == nullptr) {
            continue;
        }
        if (!thread->isTrackActive_l(track) || track->state() != IAfTrackBase::ACTIVE) {
            continue;
        }
        const float finalVolume = track->getFinalVolume();
        const bool muted = finalVolume <= 0.0f;
        if (!muted) {
            ++activeTrackCount;
        }
        if (track->isBitPerfect() && !muted) {
            ++qualifyingBitPerfectTracks;
        } else if (!muted) {
            // An active, audible track that is not a bit-perfect track breaks the verdict.
            ++unmutedNonBitPerfectTracks;
        }
        // Whether ANY active track resamples (source rate != device rate). This is a boolean fact, not
        // a picked rate: we must not single out one track's rate (getTracks_l() is pointer-address
        // order — picking "the first" would be a banned heuristic). The exact per-track source rates
        // are carried truthfully in out->sources for the UI to render.
        if (deviceRate != 0 && track->sampleRate() != deviceRate) {
            anyResampling = true;
        }
    }

    const bool bitPerfect = thread->type() == IAfThreadBase::BIT_PERFECT
            && qualifyingBitPerfectTracks == 1
            && unmutedNonBitPerfectTracks == 0;

    out->activeTrackCount = activeTrackCount;
    out->bitPerfect = bitPerfect;

    // Reasons are emitted only when the verdict is No, as discrete strings in signal order. For
    // bypass paths (direct/offload/mmap, no mixer) we add no "Mixed path" line — those are
    // single-stream by construction; only the three reasons that truly apply are added.
    out->bitPerfectReasons.clear();
    if (!bitPerfect) {
        // A mixer-bearing thread float-accumulates and requantizes. hasMixer() covers MIXER /
        // DUPLICATING / SPATIALIZER; a BIT_PERFECT thread that failed to qualify above also fell
        // back to mixing its (multiple/unmuted) tracks, so it gets the same reason. Bypass paths
        // (DIRECT/OFFLOAD/MMAP) have no mixer and are single-stream — no "Mixed path" line.
        if (thread->hasMixer() || thread->type() == IAfThreadBase::BIT_PERFECT) {
            out->bitPerfectReasons.push_back("Mixed path (float re-mix)");
        }
        if (activeTrackCount >= 2) {
            char buf[48];
            snprintf(buf, sizeof(buf), "Mixing %d active tracks", activeTrackCount);
            out->bitPerfectReasons.push_back(std::string(buf));
        }
        if (anyResampling) {
            // Rate-agnostic: a track resamples to the device rate. We deliberately do not name a single
            // source rate here (that would mean picking one of several tracks by pointer order); the
            // exact per-track rates are in out->sources.
            out->bitPerfectReasons.push_back("Resampling to device rate");
        }
    }
}

void AudioInformationService::describeMmapThread_l(
        IAfMmapThread* thread, AudioPathInfo* out) const {
    // See describePlaybackThread_l for lock ordering.
    audio_utils::lock_guard _tl(thread->mutex());

    out->ioHandle = thread->id();

    // The MMAP output flags ARE reachable: getOutput_l() is declared on IAfThreadBase, and an
    // MMAP_PLAYBACK thread carries a non-null AudioStreamOut, so its flags read exactly like a
    // playback thread's. Classification still keys off the MMAP_PLAYBACK type; the flags corroborate.
    AudioStreamOut* const output = thread->getOutput_l();
    const audio_output_flags_t flags = output != nullptr ? output->flags : AUDIO_OUTPUT_FLAG_NONE;
    out->outputFlags = static_cast<int32_t>(flags);

    // MMAP exclusive bypasses the mixer entirely: no mixer stage, no fast mixer.
    out->pathType = kPathTypeMmapExclusive;
    out->hasMixerStage = false;
    out->hasFastMixer = false;

    // For MMAP the stream is linear PCM at the HAL rate; mix and hardware values
    // coincide. There is no thread-level HAL latency accessor for MMAP, so
    // latencyMs is left at 0 rather than fabricated.
    out->hardwareSampleRate = static_cast<int32_t>(thread->sampleRate());
    out->hardwareFormat = static_cast<int32_t>(thread->format());
    out->hardwareChannelMask = static_cast<int32_t>(thread->channelMask());

    // Own sink device(s): the audio_port_handle_t of each current device (mDeviceIds) — the exact key
    // the client matches against AudioDeviceInfo.getId(). Read under the held thread mutex above.
    out->sinkPortIds = thread->outDevicePortIds_l();

    // MMAP exclusive has no mixer client-track source stage: the stream is linear PCM at the HAL rate,
    // so there is no per-track source to read here. sources stays empty (consistent with the prior
    // code that zeroed the scalar source fields), and the client renders no source stage for it.
    out->sources.clear();

    // An MMAP exclusive path is never a BIT_PERFECT thread, so the verdict is always No. It is
    // single-stream by construction: report 1 when it carries a track, else 0. No reasons apply
    // (the path nature, not a mix/resample/multi-track condition, is why it is not bit-perfect).
    out->bitPerfect = false;
    int32_t mmapTrackCount = 0;
    for (const sp<IAfTrackBase>& tb : thread->getTracks_l()) {
        if (tb != nullptr) {
            ++mmapTrackCount;
        }
    }
    out->activeTrackCount = mmapTrackCount > 0 ? 1 : 0;
    out->bitPerfectReasons.clear();

    collectEffects_l(thread, out);
}

void AudioInformationService::collectEffects_l(
        IAfThreadBase* thread, AudioPathInfo* out) const NO_THREAD_SAFETY_ANALYSIS {
    // Output-chain effects only: OUTPUT_MIX (0) and OUTPUT_STAGE (-1). Device
    // effects (AUDIO_SESSION_DEVICE) live in DeviceEffectManager, not in the
    // per-output chains, and are intentionally not reported here.
    for (const sp<IAfEffectChain>& chain : thread->getEffectChains_l()) {
        const audio_session_t sessionId = chain->sessionId();
        if (sessionId != AUDIO_SESSION_OUTPUT_MIX
                && sessionId != AUDIO_SESSION_OUTPUT_STAGE) {
            continue;
        }
        const size_t count = chain->numberOfEffects();
        for (size_t i = 0; i < count; ++i) {
            const sp<IAfEffectModule> effect = chain->getEffectModule(i);
            if (effect == nullptr) {
                continue;
            }
            const effect_descriptor_t& desc = effect->desc();
            AudioEffectInfo entry;
            entry.name = std::string(desc.name, strnlen(desc.name, sizeof(desc.name)));
            entry.typeUuid = uuidToString(desc.type);
            entry.implUuid = uuidToString(desc.uuid);
            entry.enabled = effect->isEnabled();
            entry.suspended = effect->suspended();
            out->effects.push_back(std::move(entry));
        }
    }
}

}  // namespace android
