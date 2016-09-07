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

#ifndef ANDROID_WFD_SERVICE_H_
#define ANDROID_WFD_SERVICE_H_

#include <utils/Errors.h>
#include <sys/types.h>
#include <cutils/log.h>
#include <binder/IServiceManager.h>
#include <utils/StrongPointer.h>
#include <utils/Mutex.h>

#include <media/IWFDService.h>

namespace android {

class WFDService
    : public BnWFDService {

public:
    static void instantiate();

    // add the WFDListener to WFDService.
    virtual status_t addWFDServiceListener(
                        const sp<IWFDServiceListener> &listener);

    // remove the WFDListener on WFDService.
    virtual status_t removeWFDServiceListener(
                        const sp<IWFDServiceListener> &listener);

    // send command to WFDListener.
    // sendCommand is blocking call.
    // When sendCommand of WFDService is called,
    // notify will be called in WFDListener which registered on WFDService.
    virtual status_t sendCommand(int commandType, int ext1, int ext2);

private:
    Vector<sp<IWFDServiceListener> > mWFDListeners;
    Mutex mLock;

    WFDService();
    virtual ~WFDService();
};

}
#endif
