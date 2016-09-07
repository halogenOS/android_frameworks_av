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
#define LOG_NDEBUG 0
#define LOG_TAG "WFDService"

#include <utils/Log.h>
#include <media/WFDService.h>

namespace android {

void WFDService::instantiate()
{
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm->checkService(String16("media.wifi_display")) == NULL) {
        sm->addService(String16("media.wifi_display"), new WFDService());
    }
}

WFDService::WFDService()
{

}

WFDService::~WFDService()
{

}

status_t WFDService::addWFDServiceListener(
    const sp<IWFDServiceListener> &listener)
{
    Mutex::Autolock lock(&mLock);
    mWFDListeners.push(listener);
    return OK;
}

status_t WFDService::removeWFDServiceListener(
    const sp<IWFDServiceListener> &listener)
{
    Mutex::Autolock lock(&mLock);
    for (unsigned int i = 0; i < mWFDListeners.size(); i++) {
        sp<IWFDServiceListener> tempListener = mWFDListeners.itemAt(i);
        if (listener == tempListener)
            mWFDListeners.removeAt(i);
    }
    return OK;
}

status_t WFDService::sendCommand(
    int commandType, int ext1, int ext2)
{
    Mutex::Autolock lock(&mLock);
    for (unsigned int i = 0; i < mWFDListeners.size(); i++) {
        sp<IWFDServiceListener> listener = mWFDListeners.itemAt(i);
        if (listener->getCommandType() | commandType) {
            listener->notify(commandType, ext1, ext2);
        }
    }
    return OK;
}

}
