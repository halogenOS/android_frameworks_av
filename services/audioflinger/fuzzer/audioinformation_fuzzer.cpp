/*
 * Copyright (C) 2026 The halogenOS Project
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

#include <fuzzbinder/libbinder_driver.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <utils/StrongPointer.h>

#include "AudioInformationService.h"

using android::fuzzService;
using android::sp;
using android::AudioInformationService;

// The service is constructed with a null AudioFlinger backing: its binder methods null-guard and
// return empty results, which is exactly the surface we want the fuzzer to exercise without needing
// a live audioserver. The read-only query interface is then driven with fuzzed binder transactions.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    sp<AudioInformationService> service = sp<AudioInformationService>::make(nullptr);

    fuzzService(service, FuzzedDataProvider(data, size));
    return 0;
}
