/*
 * Copyright (c) 2013 - 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _AV_EXTENSIONS_H_
#define _AV_EXTENSIONS_H_

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/SharedMemoryBuffer.h>
#include <common/AVExtensionsCommon.h>
#include <system/audio.h>
#include <media/IOMX.h>
#include <camera/android/hardware/ICamera.h>
#include <media/mediarecorder.h>
#include "ESQueue.h"

namespace android {

struct ACodec;
struct MediaCodec;
struct ALooper;
class MediaExtractor;
class AudioParameter;
class MetaData;
class CameraParameters;
class MediaBuffer;
class CameraSource;
class CameraSourceTimeLapse;
class ICameraRecordingProxy;
struct Size;
class MPEG4Writer;
struct AudioSource;

/*
 * Factory to create objects of base-classes in libstagefright
 */
class AVFactory {

public:
    AVFactory();
    ~AVFactory();

    sp<ACodec> createACodec();
    MediaExtractor* createExtendedExtractor(
            const sp<DataSource> &source, const char *mime, const sp<AMessage> &meta,
            const uint32_t flags);
    ElementaryStreamQueue* createESQueue(
            ElementaryStreamQueue::Mode mode, uint32_t flags = 0);
    CameraSource *CreateCameraSourceFromCamera(
            const sp<hardware::ICamera> &camera,
            const sp<ICameraRecordingProxy> &proxy,
            int32_t cameraId,
            const String16& clientName,
            uid_t clientUid,
            pid_t clientPid,
            Size videoSize,
            int32_t frameRate,
            const sp<IGraphicBufferProducer>& surface,
            bool storeMetaDataInVideoBuffers = true);

    CameraSourceTimeLapse *CreateCameraSourceTimeLapseFromCamera(
            const sp<hardware::ICamera> &camera,
            const sp<ICameraRecordingProxy> &proxy,
            int32_t cameraId,
            const String16& clientName,
            uid_t clientUid,
            pid_t clientPid,
            Size videoSize,
            int32_t videoFrameRate,
            const sp<IGraphicBufferProducer>& surface,
            int64_t timeBetweenFrameCaptureUs,
            bool storeMetaDataInVideoBuffers = true);
    AudioSource* createAudioSource(
            audio_source_t inputSource,
            const String16 &opPackageName,
            uint32_t sampleRate,
            uint32_t channels,
            uint32_t outSampleRate = 0,
            uid_t clientUid = -1,
            pid_t clientPid = -1);
    MPEG4Writer *CreateMPEG4Writer(int fd);

    static AVFactory* get();

private:
    static AVFactory* sInst;

};

/*
 * Common delegate to the classes in libstagefright
 */
class AVUtils {

public:
    AVUtils();
    ~AVUtils();

    status_t convertMetaDataToMessage(
            const sp<MetaData> &meta, sp<AMessage> *format);
    status_t convertMessageToMetaData(
            const sp<AMessage> &msg, sp<MetaData> &meta);
    status_t mapMimeToAudioFormat( audio_format_t& format, const char* mime);
    status_t sendMetaDataToHal(const sp<MetaData>& meta, AudioParameter *param);
    sp<MediaCodec> createCustomComponentByName(const sp<ALooper> &looper,
                const char* mime, bool encoder, const sp<AMessage> &format);
    bool isEnhancedExtension(const char *extension);

    bool hasAudioSampleBits(const sp<MetaData> &);
    bool hasAudioSampleBits(const sp<AMessage> &);
    int getAudioSampleBits(const sp<MetaData> &);
    int getAudioSampleBits(const sp<AMessage> &);
    audio_format_t updateAudioFormat(audio_format_t audioFormat,
            const sp<MetaData> &);

    audio_format_t updateAudioFormat(audio_format_t audioFormat,
            const sp<AMessage> &);
    bool canOffloadAPE(const sp<MetaData> &meta);
    bool useQCHWEncoder(const sp<AMessage> &,Vector<AString> *) { return false; }

    int32_t getAudioMaxInputBufferSize(audio_format_t audioFormat,
            const sp<AMessage> &);

    bool mapAACProfileToAudioFormat(const sp<MetaData> &,
            audio_format_t &,
            uint64_t /*eAacProfile*/);

    bool mapAACProfileToAudioFormat(const sp<AMessage> &,
            audio_format_t &,
            uint64_t /*eAacProfile*/);

    void extractCustomCameraKeys(
            const CameraParameters& /*params*/, sp<MetaData> &/*meta*/) {}
    void printFileName(int /*fd*/) {}
    void addDecodingTimesFromBatch(MediaBuffer * /*buf*/,
            List<int64_t> &/*decodeTimeQueue*/) {}

    bool canDeferRelease(const sp<MetaData> &/*meta*/) { return false; }
    void setDeferRelease(sp<MetaData> &/*meta*/) {}

    bool isAudioMuxFormatSupported(const char *mime);
    void cacheCaptureBuffers(sp<hardware::ICamera> camera, video_encoder encoder);
    void getHFRParams(bool*, int32_t*, sp<AMessage>);
    int64_t overwriteTimeOffset(bool, int64_t, int64_t *, int64_t, int32_t);
    const char *getCustomCodecsLocation();
    const char *getCustomCodecsPerformanceLocation();

    void setIntraPeriod(
                int nPFrames, int nBFrames, sp<IOMXNode> mOMXNode);

    const char *getComponentRole(bool isEncoder, const char *mime);


    // Used by ATSParser
    bool IsHevcIDR(const sp<ABuffer> &accessUnit);

    sp<AMessage> fillExtradata(sp<MediaCodecBuffer>&, sp<AMessage> &format);

    static AVUtils* get();

private:
    static AVUtils* sInst;

};

}

#endif // _AV_EXTENSIONS__H_
