// Copyright 2023 Jakub Księżniak
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "libuac.h"

namespace uac {

std::shared_ptr<uac_context> uac_context::create() {
    return std::make_shared<uac_context>(nullptr);
}

uac_context::uac_context(libusb_context *libusb_ctx) : usb_context(libusb_ctx) {
    if (libusb_ctx == nullptr) {
        libusb_init(nullptr);
        //libusb_set_option(usb_context, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
    }
}

uac_context::~uac_context() {
    if (usb_context == nullptr) {
        libusb_exit(nullptr);
    }
}

std::vector<std::shared_ptr<uac_device>> uac_context::queryAllDevices() {
    auto list = std::vector<std::shared_ptr<uac_device>>();
    libusb_device** devices = nullptr;
    
    int count = libusb_get_device_list(usb_context, &devices);
    if (count > 0) {
        for (size_t i = 0; i < count; ++i) {
            auto usb_device = devices[i];
            list.push_back(std::make_shared<uac_device>(shared_from_this(), usb_device));
        }
    }
    libusb_free_device_list(devices, 1);
    return list;
}

}

