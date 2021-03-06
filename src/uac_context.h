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

#pragma once

#include "libuac.h"
#include <thread>
#include <atomic>

namespace uac {

    class uac_context_impl : public uac_context, public std::enable_shared_from_this<uac_context> {
    public:
        uac_context_impl(libusb_context *libusb_ctx);
        ~uac_context_impl();

        virtual std::vector<std::shared_ptr<uac_device>> query_all_devices();

        virtual std::shared_ptr<uac_device_handle> wrap(int fd);
        
    private:
        libusb_context *libusb_ctx;

        std::unique_ptr<std::thread> thread;
        std::atomic<bool> alive;
    };

}