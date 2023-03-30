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

//#define VERBOSE_VLOG

#include "uac_context.h"
#include "uac_device.h"

#include <utility>
#include "uac_parser.h"
#include "uac_streaming.h"
#include "logging.h"
#include "uac_exceptions.h"

namespace uac {

    uac_device_impl::uac_device_impl(std::shared_ptr<uac_context> context, libusb_device *usb_device) : context(std::move(context)), usb_device(usb_device) {
        libusb_device_descriptor desc{};
        libusb_get_device_descriptor(usb_device, &desc);

        LOG_DEBUG("try to scan device: %04x:%04x", desc.idVendor, desc.idProduct);
        audiocontrol = uac_scan_device(usb_device);
        fix_device_quirks(desc);
        libusb_ref_device(usb_device);
    }

    uac_device_impl::~uac_device_impl() {
        LOG_VERBOSE("destructor");
        libusb_unref_device(usb_device);
        usb_device = nullptr;
    }

    void uac_device_impl::fix_device_quirks(libusb_device_descriptor &desc) {
        if (desc.idVendor == 0x534d && (desc.idProduct == 0x2109 || desc.idProduct == 0x0021)) {
            LOG_DEBUG("Apply device quirks!!");
            auto& setting = audiocontrol->streams.back();
            auto format = (uac_format_type_1*)(setting.altsettings[0].general.format.get());
            format->bNrChannels = 2;
            format->tSamFreq[0] = 48000;
            quirk_swap_channels = true;
        }
    }

    bool uac_device_impl::hasQuirkSwapChannels() const {
        return quirk_swap_channels;
    }

    uint16_t uac_device_impl::get_vid() const {
        libusb_device_descriptor desc{};
        libusb_get_device_descriptor(usb_device, &desc);
        return desc.idVendor;
    }

    uint16_t uac_device_impl::get_pid() const {
        libusb_device_descriptor desc{};
        libusb_get_device_descriptor(usb_device, &desc);
        return desc.idProduct;
    }

    std::shared_ptr<uac_device_handle> uac_device_impl::open() {
        libusb_device_handle *hDev;
        int errval = libusb_open(usb_device, &hDev);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("libusb_open()", (libusb_error)errval);
        }
        return wrapHandle(hDev);
    }

    std::shared_ptr<uac_device_handle> uac_device_impl::wrapHandle(libusb_device_handle *h_dev) {
        int errval = libusb_set_auto_detach_kernel_driver(h_dev, true);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("wrapHandle()", (libusb_error)errval);
        }
        return std::make_shared<uac_device_handle_impl>(shared_from_this(), h_dev);
    }

    std::vector<ref_uac_audio_function_topology> uac_device_impl::query_audio_function(uac_terminal_type termIn, uac_terminal_type termOut) const {
        auto& audioFunctions = audiocontrol->audio_functions();
        std::vector<ref_uac_audio_function_topology> elems;
        for (auto&& aft : audioFunctions) {
            if (aft.contains_terminal(termOut) && aft.contains_terminal(termIn)) {
                elems.emplace_back(aft);
            }
        }
        return elems;
    }

    const uac_stream_if& uac_device_impl::get_stream_interface(const uac_audio_function_topology& topology) const {
        auto topology_impl = static_cast<const uac_audio_function_topology_impl&>(topology);
        for (auto &stream : audiocontrol->streams) {
            for (auto& alt : stream.altsettings) {
                if (alt.general.bTerminalLink == topology_impl.entry->outTerminal->bTerminalID) {
                    return stream;
                }
            }
        }
        throw std::out_of_range("missing stream interface for a given topology");
    }


    uac_device_handle_impl::uac_device_handle_impl(std::shared_ptr<uac_device_impl> device, libusb_device_handle *usb_handle) : device(std::move(device)), usb_handle(usb_handle) {
        LOG_VERBOSE("constructor");
    }

    uac_device_handle_impl::~uac_device_handle_impl() {
        LOG_VERBOSE("destructor");
        close();
    }

    void uac_device_handle_impl::close() {
        LOG_ENTER();
        if (usb_handle != nullptr) {
            detach();
            LOG_VERBOSE("close %p", usb_handle);
            libusb_close(usb_handle);
            usb_handle = nullptr;
        }
    }

    void uac_device_handle_impl::detach() {
        LOG_ENTER();
        if (usb_handle != nullptr) {
            int bInterfaceNumber = device->audiocontrol->bInterfaceNumber;
            LOG_DEBUG("release AC intf(%d)", bInterfaceNumber);
            libusb_release_interface(usb_handle, bInterfaceNumber);
        }
    }

    std::shared_ptr<uac_stream_handle> uac_device_handle_impl::start_streaming(const uac_stream_if& streamIf, uint8_t setting, stream_cb_func cb_func) {
        return start_streaming(streamIf, setting, cb_func, 1, 0);
    }

    std::shared_ptr<uac_stream_handle> uac_device_handle_impl::start_streaming(const uac_stream_if& streamIf, uint8_t setting, stream_cb_func cb_func, int burst, uint32_t samplingRate) {
        auto* streamIfImpl = static_cast<const uac_stream_if_impl*>(&streamIf);
        if (setting >= streamIfImpl->altsettings.size()) throw std::invalid_argument("invalid format");
        if (burst < 1) throw std::invalid_argument("invalid burst value");

        LOG_DEBUG("claim AC intf(%d)", device->audiocontrol->bInterfaceNumber);
        int errval = libusb_claim_interface(usb_handle, device->audiocontrol->bInterfaceNumber);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("libusb_claim_interface()", (libusb_error)errval);
        }

        auto& altsetting = streamIfImpl->altsettings[setting];
        auto streamHandle = std::make_shared<uac_stream_handle_impl>(shared_from_this(), streamIfImpl->bInterfaceNr, altsetting.bAlternateSetting, altsetting.endpoint, altsetting.general);
        streamHandle->setSamplingRate(samplingRate);
        streamHandle->start(cb_func, burst);
        return streamHandle;
    }

    bool uac_device_handle_impl::is_master_muted(const uac_audio_function_topology &topology) {
        auto actualTopology = reinterpret_cast<const uac_audio_function_topology_impl&>(topology);
        const int cs = MUTE_CONTROL;
        const int cn = 0;
        const int unit = actualTopology.entry->sources[0]->unit->bUnitID;
        uint8_t data = 0;

        int errval = libusb_control_transfer(
            usb_handle,
            REQ_TYPE_IF_GET,
            REQ_GET_CUR,
            cs << 8 | cn,
            unit << 8 | device->audiocontrol->bInterfaceNumber,
            &data,
            sizeof(data),
            0 /* timeout */);

        if (errval < 0)
            throw usb_exception_impl("is_master_muted()", (libusb_error)errval);
        else
            return data;
    }

    int16_t uac_device_handle_impl::get_feature_master_volume(const uac_audio_function_topology &topology) {
        auto actualTopology = reinterpret_cast<const uac_audio_function_topology_impl&>(topology);
        const int cs = VOLUME_CONTROL;
        const int cn = 0;
        const int unit = actualTopology.entry->sources[0]->unit->bUnitID;
        int16_t data = 0;

        int errval = libusb_control_transfer(
            usb_handle,
            REQ_TYPE_IF_GET,
            REQ_GET_CUR,
            cs << 8 | cn,
            unit << 8 | device->audiocontrol->bInterfaceNumber,
            (uint8_t*) &data,
            sizeof(data),
            0 /* timeout */);

        if (errval < 0)
            throw usb_exception_impl("get_feature_master_volume", (libusb_error)errval);
        else
            return data;
    }

    std::string uac_device_handle_impl::getString(uint8_t index) const {
        std::string name;
        if (index > 0) {
            name.reserve(256);
            int result = libusb_get_string_descriptor_ascii(usb_handle, index, (unsigned char *) name.data(), 256);
            if (result < 0) {
                LOG_WARN("Failed to read string descriptor");
            }
        }
        return name;
    }

    std::string uac_device_handle_impl::getName() const {
        return getString(device->audiocontrol->iInterface);
    }

    static void dump_format(FILE *f, uac_format_type_desc *format);
    void uac_device_handle_impl::dump(FILE *f) const {
        if (f == nullptr) f = stderr;

        fprintf(f, "--- USB AUDIO DEVICE CONFIGURATION ---\n");

        fprintf(f, "Audio Control:\n");
        fprintf(f, "bcdADC: 0x%04x\n", device->audiocontrol->bcdADC);
        fprintf(f, "bInterfaceNumber: %d\n", device->audiocontrol->bInterfaceNumber);

        if (device->audiocontrol->iInterface == 0) {
            fprintf(f, "iInterface: 0\n");
        } else {
            fprintf(f, "iInterface: %s\n", getName().c_str());
        }

        fprintf(f, "Input Terminals:\n");
        for (auto&& terminal : device->audiocontrol->inputTerminals) {
            fprintf(f, "- bTerminalID: %d\n", terminal->bTerminalID);
            fprintf(f, "\twTerminalType: 0x%04x\n", terminal->wTerminalType);
            fprintf(f, "\tbAssocTerminal: %d\n", terminal->bAssocTerminal);
            fprintf(f, "\tbNrChannels: %d\n", terminal->bNrChannels);
            fprintf(f, "\twChannelConfig: 0x%04x\n", terminal->wChannelConfig);
            fprintf(f, "\tiTerminal: %d\n", terminal->iTerminal);
        }
        fprintf(f, "Units:\n");
        for (auto&& unit : device->audiocontrol->units) {
            fprintf(f, "- bUnitID: %d\n", unit->bUnitID);
            fprintf(f, "\tunitType: 0x%02x\n", unit->unitType);
            // todo print unit types
        }
        fprintf(f, "Output Terminals:\n");
        for (auto&& terminal : device->audiocontrol->outputTerminals) {
            fprintf(f, "- bTerminalID: %d\n", terminal->bTerminalID);
            fprintf(f, "\twTerminalType: 0x%04x\n", terminal->wTerminalType);
            fprintf(f, "\tbAssocTerminal: %d\n", terminal->bAssocTerminal);
            fprintf(f, "\tbSourceID: %d\n", terminal->bSourceID);
            fprintf(f, "\tiTerminal: %d\n", terminal->iTerminal);
        }

        fprintf(f, "Audio Streams:\n");
        for (auto&& as : device->audiocontrol->streams) {
            fprintf(f, "- bInterfaceNr: %d\n", as.bInterfaceNr);
            for (int i = 0; i < as.altsettings.size(); ++i) {
                auto && altsetting = as.altsettings[i];
                fprintf(f, "\taltsetting: %d\n", i+1);
                fprintf(f, "\t  bTerminalLink: %d\n", altsetting.general.bTerminalLink);
                fprintf(f, "\t  wFormatTag: 0x%04x\n", altsetting.general.wFormatTag);
                fprintf(f, "\t  bDelay: %d\n", altsetting.general.bDelay);
                dump_format(f, altsetting.general.format.get());
                fprintf(f, "\t  wMaxPacketSize: %d\n", altsetting.endpoint.wMaxPacketSize);
            }
        }
    }

    static void dump_format(FILE *f, uac_format_type_desc *format) {
        fprintf(f, "\t  bFormatType: 0x%02x\n", format->bFormatType);
        uac_format_type_1 *format1;
        switch (format->bFormatType) {
        case UAC_FORMAT_TYPE_I:
            format1 = (uac_format_type_1*)format;
            fprintf(f, "\t  bNrChannels: %d\n", format1->bNrChannels);
            fprintf(f, "\t  bSubframeSize: %d\n", format1->bSubframeSize);
            fprintf(f, "\t  bBitResolution: %d\n", format1->bBitResolution);
            fprintf(f, "\t  bSamFreqType: %d\n", format1->bSamFreqType);
            if (format1->bSamFreqType > 0) {
                for (int i = 0; i < format1->bSamFreqType; ++i) {
                    fprintf(f, "\t  tSamFreq[%d]: %d\n", i, format1->tSamFreq[i]);
                }
            } else {
                fprintf(f, "\t  tLowerSamFreq: %d\n", format1->tLowerSamFreq);
                fprintf(f, "\t  tUpperSamFreq: %d\n", format1->tUpperSamFreq);
            }
            break;
        
        default:
            break;
        }
    }
}
