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


#include "uac_context.h"
#include "uac_device.h"
#include "logging.h"
#include "uac_exceptions.h"

namespace uac {

    std::shared_ptr<uac_context> uac_context::create() {
        return std::make_shared<uac_context_impl>(nullptr);
    }

    std::shared_ptr<uac_context> uac_context::create(libusb_context *usb_ctx) {
        return std::make_shared<uac_context_impl>(usb_ctx);
    }

    uac_context_impl::uac_context_impl(libusb_context *libusb_ctx) : libusb_ctx(libusb_ctx), alive(true) {
        LOG_DEBUG("create context with usb context: %p", libusb_ctx);
        if (libusb_ctx == nullptr) {
            libusb_init(nullptr);
            //libusb_set_option(libusb_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
            
            thread = std::make_unique<std::thread>([this] {
                LOG_DEBUG("THREAD START %p", this->libusb_ctx);
                timeval tv {1, 0};
                while (this->alive) {
                    libusb_handle_events_timeout(this->libusb_ctx, &tv);
                }
                LOG_DEBUG("THREAD STOP");
            });
        }
    }

    uac_context_impl::~uac_context_impl() {
        LOG_DEBUG("destroy context with usb context: %p", libusb_ctx);
        if (libusb_ctx == nullptr) {
            // stop thread
            alive = false;
            LOG_DEBUG("JOIN THREAD");
            thread->join();

            libusb_exit(nullptr);
        }
    }

    std::vector<std::shared_ptr<uac_device>> uac_context_impl::query_all_devices() {
        auto list = std::vector<std::shared_ptr<uac_device>>();
        libusb_device** devices = nullptr;
        
        ssize_t count = libusb_get_device_list(libusb_ctx, &devices);
        if (count > 0) {
            for (size_t i = 0; i < count; ++i) {
                auto usb_device = devices[i];
                try {
                    list.push_back(std::make_shared<uac_device_impl>(shared_from_this(), usb_device));
                } catch(std::exception &e) {
                    // skip invalid devices
                }
            }
        }
        libusb_free_device_list(devices, 1);
        return list;
    }

    std::shared_ptr<uac_device_handle> uac_context_impl::wrap(int fd) {
        libusb_device_handle *hDev;
        int errval = libusb_wrap_sys_device(libusb_ctx, fd, &hDev);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("libusb_wrap_sys_device()", static_cast<libusb_error>(errval));
        }
        libusb_device *dev = libusb_get_device(hDev);

        try {
            auto uacDev = std::make_shared<uac_device_impl>(shared_from_this(), dev);
            return uacDev->wrapHandle(hDev);
        } catch (std::exception &e) {
            libusb_close(hDev);
            throw;
        }
    }

}

