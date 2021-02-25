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
#include "logging.h"
#include "uac_parser.h"

namespace uac {

    uac_device::uac_device(std::shared_ptr<uac_context> context, libusb_device *usb_device) : context(context), usb_device(usb_device) {
        libusb_ref_device(usb_device);

        libusb_device_descriptor desc;
        libusb_get_device_descriptor(usb_device, &desc);
        LOG_DEBUG("created uac_device: %x:%x", desc.idVendor, desc.idProduct);

        auto config = uac_scan_device(usb_device);
    }

    uac_device::~uac_device() {
        libusb_unref_device(usb_device);
    }

    std::shared_ptr<uac_device_handle> uac_device::open() {
        libusb_device_handle *dev_handle;
        int errval = libusb_open(usb_device, &dev_handle);
        if (errval == LIBUSB_SUCCESS) {
            
            return std::make_shared<uac_device_handle>(dev_handle);
        } else {
            throw std::exception();
        }
    }

    uac_device_handle::uac_device_handle(libusb_device_handle *dev_handle) : dev_handle(dev_handle) {}

    void uac_device_handle::close() {
        libusb_close(dev_handle);
        dev_handle = nullptr;
    }
}