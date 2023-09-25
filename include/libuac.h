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

        UAC_TERMINAL_BIDIR_UNDEFINED = 0x400,
        UAC_TERMINAL_HANDSET = 0x401,
        UAC_TERMINAL_HEADSET = 0x402,
        UAC_TERMINAL_SPEAKERPHONE = 0x403,
        UAC_TERMINAL_SPEAKERPHONE_ECHO_SUPPRESSING = 0x404,
        UAC_TERMINAL_SPEAKERPHONE_ECHO_CANCELLING = 0x405,

        UAC_TERMINAL_EXTERNAL_UNDEFINED = 0x600,
        UAC_TERMINAL_EXTERNAL_ANALOG = 0x601,
        UAC_TERMINAL_EXTERNAL_DIGITAL = 0x602,

        UAC_TERMINAL_ANY = 0xF00,
    };

    /**
     * (Frmts) Table A.1-3 Audio Data Format Type I-III Codes
     */
    enum uac_audio_data_format_type : uint16_t {
        UAC_FORMAT_DATA_TYPE_I_UNDEFINED = 0x0000,
        UAC_FORMAT_DATA_PCM = 0x0001,
        UAC_FORMAT_DATA_PCM8 = 0x0002,
        UAC_FORMAT_DATA_IEEE_FLOAT = 0x0003,
        UAC_FORMAT_DATA_ALAW = 0x0004,
        UAC_FORMAT_DATA_MULAW = 0x0005,

        UAC_FORMAT_DATA_TYPE_II_UNDEFINED = 0x1000,
        UAC_FORMAT_DATA_MPEG = 0x1001,
        UAC_FORMAT_DATA_AC3 = 0x1002,

        UAC_FORMAT_DATA_TYPE_III_UNDEFINED = 0x2000,
        UAC_FORMAT_DATA_IEC1937_AC3 = 0x2001,
        UAC_FORMAT_DATA_IEC1937_MPEG1 = 0x2002,
        UAC_FORMAT_DATA_IEC1937_MPEG2 = 0x2003,
        UAC_FORMAT_DATA_IEC1937_MPEG2_EXT = 0x2004,
        UAC_FORMAT_DATA_IEC1937_MPEG2_L1_LS = 0x2005,
        UAC_FORMAT_DATA_IEC1937_MPEG2_L2_LS = 0x2006,

        UAC_FORMAT_DATA_ANY = 0xFFFF
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
        /**
         * @brief Creates a new context using the default LibUSB context.
         * 
         * It will create a new thread that will handle USB events.
         * 
         * @return std::shared_ptr<uac_context> 
         */
        static std::shared_ptr<uac_context> create();

        /**
         * @brief Creates a new context using the given LibUSB context.
         * 
         * Make sure to setup your own event handling loop.
         * 
         * @param usb_ctx 
         * @return std::shared_ptr<uac_context> 
         */
        static std::shared_ptr<uac_context> create(libusb_context *usb_ctx);
        virtual ~uac_context() = default;

        /**
         * @brief Queries all devices which support USB Audio Class.
         * 
         * @return std::vector<std::shared_ptr<uac_device>> 
         */
        virtual std::vector<std::shared_ptr<uac_device>> query_all_devices() = 0;

        /**
         * @brief Wraps an already opened device file descriptor.
         * 
         * This is a useful method to access USB device on Android.
         * 
         * @param fd 
         * @return std::shared_ptr<uac_device_handle> 
         */
        virtual std::shared_ptr<uac_device_handle> wrap(int fd) = 0;
    };

    class uac_audio_route;
    class uac_stream_if;
    using ref_uac_audio_route = std::reference_wrapper<const uac_audio_route>;

    /**
     * The USB Audio device representation.
     */
    class uac_device {
    public:
        virtual ~uac_device() = default;
        /**
         * @brief Get the Vendor ID
         * 
         * @return uint16_t 
         */
        virtual uint16_t get_vid() const = 0;
        /**
         * @brief Get the Product ID
         * 
         * @return uint16_t 
         */
        virtual uint16_t get_pid() const = 0;

        /**
         * @brief Opens this device for audio streaming.
         * 
         * @return std::shared_ptr<uac_device_handle> 
         */
        virtual std::shared_ptr<uac_device_handle> open() = 0;

        /**
         * @brief Queries audio routes based on I/O terminals.
         * 
         * Audio routes may contain many different units, but they always begin and end with an input terminal and an output terminal.
         * Each Audio device supports at least one audio route, so this method allows selecting any specific route.
         * 
         * The UAC_TERMINAL_USB_STREAMING output terminal is usually used for a recording device, ie. microphone.
         * The UAC_TERMINAL_USB_STREAMING input terminal is usually used for a speaker device.
         * 
         * @param termIn 
         * @param termOut 
         * @return std::vector<ref_uac_audio_route> 
         */
        virtual std::vector<ref_uac_audio_route> query_audio_routes(uac_terminal_type termIn, uac_terminal_type termOut) const = 0;

        /**
         * @brief Get the stream interface based on the given route.
         * 
         * The stream interface is required for audio streaming through USB.
         * 
         * @param route 
         * @return const uac_stream_if& 
         */
        virtual const uac_stream_if& get_stream_interface(const uac_audio_route& route) const = 0;
    };

    /**
     * @brief Configuration for uncompressed audio streaming
     *
     */
    struct uac_audio_config_uncompressed {
        const uac_audio_data_format_type audioDataFormat;
        uint8_t bAlternateSetting;
        const uint8_t bSubframeSize;
        const uint8_t bBitResolution;
        const uint8_t bChannelCount;
        const uint16_t wMaxPacketSize;
        uint32_t tSampleRate;
    };

    struct uac_audio_config_compressed {
    };

    class uac_stream_if {
    public:
        virtual ~uac_stream_if() = default;
        virtual std::vector<uac_audio_data_format_type> get_audio_formats() const = 0;
        virtual std::vector<uint8_t> get_channel_counts(uac_audio_data_format_type fmt) const = 0;
        virtual std::vector<uint32_t> get_sample_rates(uac_audio_data_format_type fmt) const = 0;
        virtual std::vector<uint8_t> get_bit_resolutions(uac_audio_data_format_type fmt) const = 0;

        virtual std::unique_ptr<const uac_audio_config_uncompressed> query_config_uncompressed(uac_audio_data_format_type audioDataFormatType,
                                                                         uint8_t numChannels,
                                                                         uint32_t sampleRate) const = 0;
    };

    class uac_stream_handle;
    using stream_cb_func = std::function<void(uint8_t*, uint)>;

    /**
     * The device can be operated through this handle.
     */
    class uac_device_handle {
    public:
        virtual ~uac_device_handle() = default;
        virtual void close() = 0;
        virtual std::shared_ptr<uac_device> get_device() const = 0;
        virtual std::shared_ptr<uac_stream_handle> start_streaming(const uac_stream_if& streamIf, const uac_audio_config_uncompressed& config, stream_cb_func cb_func) = 0;
        virtual std::shared_ptr<uac_stream_handle> start_streaming(const uac_stream_if& streamIf, const uac_audio_config_uncompressed& config, stream_cb_func cb_func, int burst) = 0;
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
        virtual ~uac_stream_handle() = default;
        virtual void stop() = 0;
        virtual void set_sampling_rate(const uint32_t samplingRate) = 0;
    };

    /**
     * @brief
     *
     */
    class uac_audio_route {
    public:
        virtual ~uac_audio_route() = default;
        virtual bool contains_terminal(uac_terminal_type terminalType) const = 0;
    };
}
