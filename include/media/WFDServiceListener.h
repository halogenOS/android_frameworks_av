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

#ifndef ANDROID_WFD_SERVICE_LISTENER_H_
#define ANDROID_WFD_SERVICE_LISTENER_H_

#include <utils/Errors.h>
#include <sys/types.h>
#include <cutils/log.h>
#include <binder/IServiceManager.h>
#include <utils/StrongPointer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/IWFDService.h>

namespace android {

typedef void (*NotifyFunc)(void *me, int commandType, int ext1, int ext2);

class WFDServiceListener
    : public BnWFDServiceListener {

public:
    WFDServiceListener(void *me, NotifyFunc notifyFunc, int commandType);

    // When sendCommand of WFDService is called,
    // notify will be called in WFDListener which registered on WFDService.
    virtual void notify(int commandType, int ext1, int ext2);
    virtual int getCommandType();

private:
    void *mCookie;
    NotifyFunc mNotifyFunc;
    int mCommandType;

    virtual ~WFDServiceListener();
};

}
#endif
