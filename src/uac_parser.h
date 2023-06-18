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
#include "usb_audio.h"
#include "libuac.h"

/** Converts an unaligned two-byte little-endian integer into an int16 */
#define TO_WORD(p) ((uint8_t)(p)[0] | \
                       ((int8_t)(p)[1] << 8))

/** Converts an unaligned four-byte little-endian integer into an int32 */
#define TO_DWORD(p) ((uint8_t)(p)[0] | \
                     ((uint8_t)(p)[1] << 8) | \
                     ((uint8_t)(p)[2] << 16) | \
                     ((int8_t)(p)[3] << 24))

/** Converts an unaligned three-byte little-endian integer into an int32 */
#define TO_DWORD24(p) ((uint8_t)(p)[0] | \
                     ((uint8_t)(p)[1] << 8) | \
                     ((uint8_t)(p)[2] << 16))

#define H_DWORD24(p) {(uint8_t)((p) & 0xff), \
                     (uint8_t)((p) >> 8), \
                     (uint8_t)((p) >> 16)}

/** Converts an unaligned eight-byte little-endian integer into an int64 */
#define TO_QWORD(p) (((uint64_t)(p)[0]) | \
                      (((uint64_t)(p)[1]) << 8) | \
                      (((uint64_t)(p)[2]) << 16) | \
                      (((uint64_t)(p)[3]) << 24) | \
                      (((uint64_t)(p)[4]) << 32) | \
                      (((uint64_t)(p)[5]) << 40) | \
                      (((uint64_t)(p)[6]) << 48) | \
                      (((int64_t)(p)[7]) << 56))

namespace uac {
    
    struct uac_topology_entity {
        uac_topology_entity* sink = nullptr;
        std::vector<uac_topology_entity*> sources;

        // either
        std::shared_ptr<uac_unit> unit;
        std::shared_ptr<uac_input_terminal> inTerminal;
        std::shared_ptr<uac_output_terminal> outTerminal;

        uac_topology_entity(std::shared_ptr<uac_unit> unit);
        uac_topology_entity(std::shared_ptr<uac_input_terminal> inTerminal);
        uac_topology_entity(std::shared_ptr<uac_output_terminal> outTerminal);
        ~uac_topology_entity();

        std::vector<int> source_ids() const;

        uac_topology_entity* link_source(std::shared_ptr<uac_unit> srcUnit);
        uac_topology_entity* link_source(std::shared_ptr<uac_input_terminal> terminal);
    };

    class uac_audio_route_impl : public uac_audio_route {
    public:
        uac_audio_route_impl(std::shared_ptr<uac_topology_entity> entry);
        bool contains_terminal(uac_terminal_type terminalType) const override;

        std::shared_ptr<uac_topology_entity> entry;
    private:
        static uac_topology_entity* findInputTerminalByType(uac_topology_entity *entity, uac_terminal_type terminalType);
    };

    struct uac_endpoint_desc {
        uint8_t bEndpointAddress;
        uint16_t wMaxPacketSize;
        iso_endpoint_desc iso_desc;
    };

    struct uac_altsetting {
        uint8_t bAlternateSetting;
        uac_as_general general;
        uac_endpoint_desc endpoint;

        bool supportsSampleRate(int32_t sampleRate) const;
    };

    class uac_stream_if_impl : public uac_stream_if {
    public:
        uac_stream_if_impl(uint8_t bInterfaceNr) : bInterfaceNr(bInterfaceNr) {}
        int find_stream_setting(int32_t sampleRate) const override;
        int get_bytes_per_transfer(uint8_t settingIdx) const override;
        std::vector<uac_format> get_formats() const override;

        uint8_t bInterfaceNr;
        std::vector<uac_altsetting> altsettings;
    };

    class uac_audiocontrol : public uac_ac_header {
    public:
        uac_audiocontrol(uint8_t bInterfaceNr, uint8_t iInterface) : bInterfaceNumber(bInterfaceNr), iInterface(iInterface) {}
        void configure_audio_function();
        const std::vector<uac_audio_route_impl>& audio_routes() {
            return audioFunctionTopology;
        }

        std::vector<uac_stream_if_impl> streams;

        std::vector<std::shared_ptr<uac_input_terminal>> inputTerminals;
        std::vector<std::shared_ptr<uac_output_terminal>> outputTerminals;
        std::vector<std::shared_ptr<uac_unit>> units;

        const uint8_t bInterfaceNumber;
        const uint8_t iInterface;
    private:
        uac_audio_route_impl build_audio_topology(std::shared_ptr<uac_output_terminal> outputTerminal);
        std::shared_ptr<uac_unit> find_unit(int id);
        std::shared_ptr<uac_input_terminal> find_input_terminal(int id);

        std::vector<uac_audio_route_impl> audioFunctionTopology;
    };

    std::unique_ptr<uac_audiocontrol> uac_scan_device(libusb_device *udev);

    void parse_ac_header(uac_audiocontrol& ac, const uint8_t *data, int size);
    std::shared_ptr<uac_input_terminal> parse_input_terminal(const uint8_t *data, int size);
    std::shared_ptr<uac_output_terminal> parse_output_terminal(const uint8_t *data, int size);
    std::shared_ptr<uac_mixer_unit> parse_mixer_unit(const uint8_t *data, int size);
    std::shared_ptr<uac_feature_unit> parse_feature_unit(const uint8_t *data, int size);
}
