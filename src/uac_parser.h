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

#include <libusb-1.0/libusb.h>
#include <memory>
#include <vector>
#include "usb_audio.h"

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
    struct uac_config;
    
    struct uac_stream_if;

    struct uac_audiocontrol : uac_ac_header {
        std::vector<uac_stream_if> streams;
    };

    struct uac_stream_if {
        uint8_t bInterfaceNr;
        uac_as_general *streamSettings;

        uac_stream_if(int interfaceNr) : bInterfaceNr(interfaceNr) {}
    };


    std::unique_ptr<uac_config> uac_scan_device(libusb_device *udev);

    std::shared_ptr<uac_audiocontrol> parse_audiocontrol(const libusb_interface_descriptor *ifdesc);

    void parse_header(std::shared_ptr<uac_audiocontrol> ac, const uint8_t *data, int size);

    class uac_parser {
    private:
        /* data */
    public:
        uac_parser(/* args */);
        ~uac_parser();
    };

    class uac_config {
        libusb_config_descriptor *config;
    public:
        std::shared_ptr<uac_audiocontrol> audiocontrol;

    public:
        uac_config(libusb_config_descriptor *config) : config(config) {}
        ~uac_config();
    };


    std::shared_ptr<uac_input_terminal> parse_input_terminal(const uint8_t *data, int size);
    std::shared_ptr<uac_output_terminal> parse_output_terminal(const uint8_t *data, int size);
    std::shared_ptr<uac_mixer_unit> parse_mixer_unit(const uint8_t *data, int size);
    std::shared_ptr<uac_feature_unit> parse_feature_unit(const uint8_t *data, int size);
    

    class uac_topology {
        uac_input_terminal input;
        uac_output_terminal output;
    };
}
