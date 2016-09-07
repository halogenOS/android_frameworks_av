/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_IWFDSERVICE_H
#define ANDROID_IWFDSERVICE_H

#include <binder/IInterface.h>
#include <media/stagefright/foundation/ABase.h>

namespace android {

#define WFD_ENCODER_EXT1_UNLOAD         (1)
#define WFD_ENCODER_EXT1_LOAD_NORMAL    (2)
#define WFD_ENCODER_EXT1_LOAD_SECURE    (3)

#define WFD_ENCODER_CMD                 (1 << 0)

struct IWFDServiceListener : public IInterface {
    DECLARE_META_INTERFACE(WFDServiceListener);

    // When sendCommand of WFDService is called,
    // notify will be called on WFDListener which registered on WFDService.
    virtual void notify(int msg, int ext1, int ext2) = 0;
    virtual int getCommandType() = 0;
};

struct IWFDService : public IInterface {
    DECLARE_META_INTERFACE(WFDService);

    // add the WFDListener to WFDService.
    virtual status_t addWFDServiceListener(
                        const sp<IWFDServiceListener> &listener) = 0;

    // remove the WFDListener on WFDService.
    virtual status_t removeWFDServiceListener(
                        const sp<IWFDServiceListener> &listener) = 0;

    // send command to WFDListener.
    // sendCommand is blocking call.
    virtual status_t sendCommand(int commandType, int ext1, int ext2) = 0;
};

struct BnWFDServiceListener : public BnInterface<IWFDServiceListener> {
    virtual status_t onTransact(
            uint32_t code, const Parcel &data, Parcel *reply,
            uint32_t flags = 0);
};

struct BnWFDService : public BnInterface<IWFDService> {
    virtual status_t onTransact(
            uint32_t code, const Parcel &data, Parcel *reply,
            uint32_t flags = 0);
};

}  // namespace android

#endif // ANDROID_IWFDSERVICE_H
