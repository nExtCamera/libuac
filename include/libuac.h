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

#include <libusb.h>
#include <memory>
#include <vector>
#include <functional>

namespace uac {

    /**
     * @brief The exception class for usb errors
     * 
     * This exception holds an error code from libusb calls.
     */
    class usb_exception : public std::exception {
    public:
        /**
         * @brief A detailed message
         * 
         * @return const char* 
         */
        virtual const char* what() const noexcept override = 0;

        /**
         * @brief The libusb error code
         * 
         * @return libusb_error 
         */
        virtual libusb_error error_code() const = 0;
    };

    /**
     * @brief Termt10. Terminal Types
     */
    enum uac_terminal_type {
        UAC_TERMINAL_USB_UNDEFINED = 0x100,
        UAC_TERMINAL_USB_STREAMING = 0x101,
        UAC_TERMINAL_USB_VENDOR_SPEC = 0x1FF,

        UAC_TERMINAL_INPUT_UNDEFINED = 0x200,
        UAC_TERMINAL_MICROPHONE = 0x201,
        UAC_TERMINAL_DESKTOP_MIC = 0x202,
        UAC_TERMINAL_PERSONAL_MIC = 0x203,
        UAC_TERMINAL_OMNIDIR_MIC = 0x204,
        UAC_TERMINAL_MIC_ARRAY = 0x205,
        UAC_TERMINAL_PROC_MIC_ARRAY = 0x206,

        UAC_TERMINAL_OUTPUT_UNDEFINED = 0x300,
        UAC_TERMINAL_SPEAKER = 0x301,
        UAC_TERMINAL_HEADPHONES = 0x302,
        UAC_TERMINAL_HMD_AUDIO = 0x303,
        UAC_TERMINAL_DESKTOP_SPEAKER = 0x304,
        UAC_TERMINAL_ROOM_SPEAKER = 0x305,
        UAC_TERMINAL_COMM_SPEAKER = 0x306,
        UAC_TERMINAL_LFR_SPEAKER = 0x307,

        UAC_TERMINAL_EXTERNAL_UNDEFINED = 0x600,
        UAC_TERMINAL_EXTERNAL_ANALOG = 0x601,
        UAC_TERMINAL_EXTERNAL_DIGITAL = 0x602,

        UAC_TERMINAL_ANY = 0xF00,
    };

    class uac_device;
    class uac_device_handle;

    /**
     * @brief The libuac context
     * 
     * All events and resources are managed under this context.
     * Usually, a single context should be enough for most use cases.
     */
    class uac_context {
    public:
        virtual ~uac_context() = default;
        virtual std::vector<std::shared_ptr<uac_device>> query_all_devices() = 0;
        
        virtual std::shared_ptr<uac_device_handle> wrap(int fd) = 0;

        static std::shared_ptr<uac_context> create();
        static std::shared_ptr<uac_context> create(libusb_context *usb_ctx);
    };

    class uac_audio_route;
    class uac_stream_if;
    using ref_uac_audio_route = std::reference_wrapper<const uac_audio_route>;

    /**
     * The Audio device representation.
     */
    class uac_device {
    public:
        virtual uint16_t get_vid() const = 0;
        virtual uint16_t get_pid() const = 0;

        virtual std::shared_ptr<uac_device_handle> open() = 0;

        virtual std::vector<ref_uac_audio_route> query_audio_routes(uac_terminal_type termIn, uac_terminal_type termOut) const = 0;

        virtual const uac_stream_if& get_stream_interface(const uac_audio_route& route) const = 0;
    };

    struct uac_format_type_desc;
    /**
     * @brief Audio format.
     * 
     */
    struct uac_format {
        uint16_t wFormatTag;
        std::shared_ptr<uac_format_type_desc> pFormatDesc;

        uint8_t get_num_channels() const;
        uint8_t get_subframe_size() const;
        uint8_t get_bit_resolution() const;

    };

    class uac_stream_if {
    public:
        virtual int find_stream_setting(int32_t sampleRate) const = 0;
        virtual int get_bytes_per_transfer(uint8_t settingIdx) const = 0;
        virtual std::vector<uac_format> get_formats() const = 0;
    };

    class uac_stream_handle;
    using stream_cb_func = std::function<void(uint8_t*, int)>;

    /**
     * The device can be operated through this handle.
     */
    class uac_device_handle {
    public:
        virtual void close() = 0;
        virtual std::shared_ptr<uac_device> get_device() const = 0;
        virtual std::shared_ptr<uac_stream_handle> start_streaming(const uac_stream_if& streamIf, uint8_t setting, stream_cb_func cb_func) = 0;
        virtual std::shared_ptr<uac_stream_handle> start_streaming(const uac_stream_if& streamIf, uint8_t setting, stream_cb_func cb_func, int burst, uint32_t samplingRate) = 0;
        virtual void detach() = 0;

        virtual std::string get_name() const = 0;

        virtual bool is_master_muted(const uac_audio_route &route) = 0;
        virtual int16_t get_feature_master_volume(const uac_audio_route &route) = 0;

        virtual void dump(FILE *f) const = 0;
    };

    /**
     * A handle to the opened audio stream.
     */
    class uac_stream_handle {
    public:
        virtual void stop() = 0;
        virtual void set_sampling_rate(const uint32_t samplingRate) = 0;
    };

    /**
     * @brief 
     * 
     */
    class uac_audio_route {
    public:
        virtual bool contains_terminal(uac_terminal_type terminalType) const = 0;
    };
}
