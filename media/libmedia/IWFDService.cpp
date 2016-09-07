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

//#define LOG_NDEBUG 0
#define LOG_TAG "IWFDService"
#include <utils/Log.h>

#include <binder/Parcel.h>
#include <media/IWFDService.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/foundation/ADebug.h>

namespace android {

enum {
    WFD_LISTENER_NOTIFY = IBinder::FIRST_CALL_TRANSACTION,
    WFD_LISTENER_GET_COMMAND_TYPE,
    WFD_ADD_LISTENER,
    WFD_REMOVE_LISTENER,
    WFD_SEND_COMMAND,
};

struct BpWFDServiceListener : public BpInterface<IWFDServiceListener> {
    BpWFDServiceListener(const sp<IBinder> &impl)
        : BpInterface<IWFDServiceListener>(impl) {
    }

    virtual void notify(int commandType, int ext1, int ext2) {
        Parcel data, reply;
        data.writeInterfaceToken(IWFDServiceListener::getInterfaceDescriptor());
        data.writeInt32(commandType);
        data.writeInt32(ext1);
        data.writeInt32(ext2);
        remote()->transact(WFD_LISTENER_NOTIFY, data, &reply);
    }

    virtual int getCommandType() {
        Parcel data, reply;
        data.writeInterfaceToken(IWFDServiceListener::getInterfaceDescriptor());
        remote()->transact(WFD_LISTENER_GET_COMMAND_TYPE, data, &reply);
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(WFDServiceListener, "android.hardware.IWFDServiceListener");

struct BpWFDService : public BpInterface<IWFDService> {
    BpWFDService(const sp<IBinder> &impl)
        : BpInterface<IWFDService>(impl) {
    }

    virtual status_t addWFDServiceListener(
                        const sp<IWFDServiceListener> &listener) {
        Parcel data, reply;
        data.writeInterfaceToken(BpWFDService::getInterfaceDescriptor());
        data.writeStrongBinder(IInterface::asBinder(listener));
        remote()->transact(WFD_ADD_LISTENER, data, &reply);
        return reply.readInt32();
    }

    virtual status_t removeWFDServiceListener(
                        const sp<IWFDServiceListener> &listener) {
        Parcel data, reply;
        data.writeInterfaceToken(BpWFDService::getInterfaceDescriptor());
        data.writeStrongBinder(IInterface::asBinder(listener));
        remote()->transact(WFD_REMOVE_LISTENER, data, &reply);
        return reply.readInt32();
    }

    virtual status_t sendCommand(int commandType, int ext1, int ext2) {
        Parcel data, reply;
        data.writeInterfaceToken(BpWFDService::getInterfaceDescriptor());
        data.writeInt32(commandType);
        data.writeInt32(ext1);
        data.writeInt32(ext2);
        remote()->transact(WFD_SEND_COMMAND, data, &reply);
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(WFDService, "android.hardware.IWFDService");

status_t BnWFDServiceListener::onTransact(
        uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags) {
    switch (code) {
        case WFD_LISTENER_NOTIFY:
        {
            CHECK_INTERFACE(IWFDServiceListener, data, reply);

            int commandType = data.readInt32();
            int ext1 = data.readInt32();
            int ext2 = data.readInt32();

            notify(commandType, ext1, ext2);

            return OK;
        }

        case WFD_LISTENER_GET_COMMAND_TYPE:
        {
            CHECK_INTERFACE(IWFDServiceListener, data, reply);

            reply->writeInt32(getCommandType());
            return OK;
        }

        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

status_t BnWFDService::onTransact(
        uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags) {
    switch (code) {
        case WFD_ADD_LISTENER:
        {
            CHECK_INTERFACE(IWFDService, data, reply);

            sp<IWFDServiceListener> listener =
                interface_cast<IWFDServiceListener>(data.readStrongBinder());

            reply->writeInt32(addWFDServiceListener(listener));
            return OK;
        }

        case WFD_REMOVE_LISTENER:
        {
            CHECK_INTERFACE(IWFDService, data, reply);

            sp<IWFDServiceListener> listener =
                interface_cast<IWFDServiceListener>(data.readStrongBinder());

            reply->writeInt32(removeWFDServiceListener(listener));
            return OK;
        }

        case WFD_SEND_COMMAND:
        {
            CHECK_INTERFACE(IWFDService, data, reply);

            int commandType = data.readInt32();
            int ext1 = data.readInt32();
            int ext2 = data.readInt32();
            reply->writeInt32(sendCommand(commandType, ext1, ext2));
            return OK;
        }

        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

}  // namespace android
