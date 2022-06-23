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

namespace uac {

    class uac_audiocontrol;

    class uac_device_impl : public uac_device, public std::enable_shared_from_this<uac_device_impl> {
    public:
        uac_device_impl(std::shared_ptr<uac_context> context, libusb_device *usb_device);
        ~uac_device_impl();

        uint16_t get_vid() const override;
        uint16_t get_pid() const override;

        std::shared_ptr<uac_device_handle> open() override;

        std::vector<ref_uac_audio_function_topology> query_audio_function(uac_terminal_type termIn, uac_terminal_type termOut) const override;

        const uac_stream_if& get_stream_interface(const uac_audio_function_topology& topology) const override;

        std::shared_ptr<uac_device_handle> wrapHandle(libusb_device_handle *h_dev);

    private:
        void fix_device_quirks(libusb_device_descriptor &desc);

        libusb_device *usb_device;
        std::shared_ptr<uac_context> context;
        std::unique_ptr<uac_audiocontrol> audiocontrol;

        friend class uac_device_handle_impl;
    };

    class uac_device_handle_impl : public uac_device_handle, public std::enable_shared_from_this<uac_device_handle_impl> {
    public:
        uac_device_handle_impl(std::shared_ptr<uac_device_impl> device, libusb_device_handle *usb_handle);
        ~uac_device_handle_impl();

        std::shared_ptr<uac_device> get_device() const override {
            return device;
        }

        void close() override;
        void detach() override;

        std::shared_ptr<uac_stream_handle> start_streaming(const uac_stream_if& streamIf, uint8_t setting, stream_cb_func cb_func) override;
        std::shared_ptr<uac_stream_handle> start_streaming(const uac_stream_if& streamIf, uint8_t setting, stream_cb_func cb_func, int burst, uint32_t samplingRate) override;

        std::string getName() const override;

        bool is_master_muted(const uac_audio_function_topology &topology) override;
        int16_t get_feature_master_volume(const uac_audio_function_topology &topology) override;

        void dump(FILE *f) const override;

    private:
        std::string getString(uint8_t index) const;

        libusb_device_handle *usb_handle;

        std::shared_ptr<uac_device_impl> device;

        friend class uac_stream_handle_impl;
    };
}
