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
#include "uac_device.h"
#include "uac_parser.h"
#include <mutex>
#include <condition_variable>

namespace uac {

    class uac_stream_handle_impl : public uac_stream_handle {
    public:
        uac_stream_handle_impl(std::shared_ptr<uac_device_handle_impl> dev_handle, uint8_t interfaceNr, uint8_t altsetting, const uac_endpoint_desc &endpointDesc, const uac::uac_as_general& settings);
        ~uac_stream_handle_impl();

        void start(stream_cb_func cb_func);
        void stop() override;
        
        stream_cb_func cb_func;

        std::mutex mMutex;
        std::condition_variable mCv;
        int mActiveTransfers;
    protected:
        void set_sampling_freq(uint32_t sampling);
        uint32_t get_sampling_freq();

    private:
        std::shared_ptr<uac_device_handle_impl> dev_handle;

        const uac::uac_as_general& settings;
        uint8_t bInterfaceNr;
        uint8_t current_altsetting;
        uac_endpoint_desc endpoint_desc;

        bool active = false;
        std::vector<libusb_transfer*> transfers;
    };
}
