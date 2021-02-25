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

#include "uac_parser.h"
#include "logging.h"
#include "usb_audio.h"

namespace uac {

    uac_config::~uac_config() {
        libusb_free_config_descriptor(config);
    }

    uac_parser::uac_parser(/* args */)
    {
    }
    
    uac_parser::~uac_parser()
    {
    }

    static void parse_as_intf(uac_stream_if &stream_if, const libusb_interface_descriptor *altsettings, int num_altsetting);

    static void scan_audiostreaming(std::shared_ptr<uac_audiocontrol> ac, const libusb_interface *usbintf) {
        for (auto &&stream : ac->streams) {
            auto ifdesc = usbintf->altsetting;
            if (stream.bInterfaceNr == ifdesc->bInterfaceNumber) {
                parse_as_intf(stream, usbintf->altsetting, usbintf->num_altsetting);
                return;
            }
        }
        LOG_DEBUG("This AudioStreaming interface is not part of current AudioControl.");
    }

    std::unique_ptr<uac_config> uac_scan_device(libusb_device *udev) {
        libusb_config_descriptor *config;
        int errval = libusb_get_active_config_descriptor(udev, &config);
        if (errval != LIBUSB_SUCCESS) {
            return nullptr;
        }
        auto uconfig = std::make_unique<uac_config>(config);
        for (size_t i = 0; i < config->bNumInterfaces; ++i) {
            auto intf_desc = config->interface[i].altsetting;
            if (intf_desc->bInterfaceClass == LIBUSB_CLASS_AUDIO) {
                LOG_DEBUG("found AUDIO Class interface, subclass=0x%x, protocol=%d", intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol);
                switch (intf_desc->bInterfaceSubClass) {
                case UAC_SUBCLASS_AUDIOCONTROL:
                    uconfig->audiocontrol = parse_audiocontrol(intf_desc);
                    break;
                case UAC_SUBCLASS_AUDIOSTREAMING:
                    scan_audiostreaming(uconfig->audiocontrol, &config->interface[i]);
                    break;
                default:
                    break;
                }
            }
        }
        
        return uconfig;
    }

    std::shared_ptr<uac_audiocontrol> parse_audiocontrol(const libusb_interface_descriptor *ifdesc) {
        auto data = ifdesc->extra;
        int remaining = ifdesc->extra_length;

        if (data == nullptr || remaining < 3) {
            LOG_ERROR("no extra data available for a given interface: bInterfaceNumber=%d", ifdesc->bInterfaceNumber);
            return nullptr;
        }
        int descSize = data[0];
        int descriptorType = data[1];
        int subtype = data[2];
        if (subtype != UAC_AC_HEADER || descSize < 8) {
            LOG_DEBUG("expected a HEADER first but got an invalid descriptor sizeof(%d) %d:%d", descSize, descriptorType, subtype);
            return nullptr;
        }
        LOG_DEBUG("got HEADER descriptor. sizeof(%d)", descSize);
        auto audiocontrol = std::make_shared<uac_audiocontrol>();
        parse_header(audiocontrol, data, descSize);

        if (audiocontrol->wTotalLength != remaining) {
            LOG_ERROR("wTotalLength mismatch with actual data available: %d != %d", audiocontrol->wTotalLength, remaining);
        }
        
        remaining -= descSize;
        data += descSize;

        // parse other descriptors
        while (remaining >= 3) {
            descSize = data[0];
            descriptorType = data[1];
            subtype = data[2];
            if (remaining < descSize) {
                LOG_ERROR("Bad descriptor size, exceeds remaining bytes %d < %d", remaining, descSize);
                break;
            }
            LOG_DEBUG("got descriptor sizeof(%d) %d:%d", descSize, descriptorType, subtype);
            switch (subtype) {
            case UAC_AC_HEADER:
                LOG_DEBUG("got another HEADER descriptor. A bug or buggy device?");
                break;
            case UAC_AC_INPUT_TERMINAL:
                parse_input_terminal(data, descSize);
                break;
            case UAC_AC_OUTPUT_TERMINAL:
                parse_output_terminal(data, descSize);
                break;
            case UAC_AC_MIXER_UNIT:
                parse_mixer_unit(data, descSize);
                break;
            case UAC_AC_FEATURE_UNIT:
                parse_feature_unit(data, descSize);
                break;
            
            default:
                break;
            }
            remaining -= descSize;
            data += descSize;
        }
        return audiocontrol;
    }

    void parse_header(std::shared_ptr<uac_audiocontrol> ac, const uint8_t *data, int size) {
        ac->bcdADC = TO_WORD(data+3);
        ac->wTotalLength = TO_WORD(data+5);
        uint8_t bInCollection = data[7];
        for (size_t i = 0; i < bInCollection; ++i) {
            ac->streams.push_back(uac_stream_if(data[8+i]));
            LOG_DEBUG("\t got stream interface at: %d", ac->streams.back().bInterfaceNr);
        }
    }

    std::shared_ptr<uac_input_terminal> parse_input_terminal(const uint8_t *data, int size) {
        auto terminal = std::make_shared<uac_input_terminal>();
        terminal->bTerminalID = data[3];
        terminal->wTerminalType = TO_WORD(data+4);
        terminal->bAssocTerminal = data[6];
        terminal->bNrChannels = data[7];
        terminal->wChannelConfig = TO_WORD(data+8);
        terminal->iChannelNames = data[10];
        terminal->iTerminal = data[11];
        LOG_DEBUG("\t got INPUT terminal %d: type=%x", terminal->bTerminalID, terminal->wTerminalType);
        return terminal;
    }

    std::shared_ptr<uac_output_terminal> parse_output_terminal(const uint8_t *data, int size) {
        auto terminal = std::make_shared<uac_output_terminal>();
        terminal->bTerminalID = data[3];
        terminal->wTerminalType = TO_WORD(data+4);
        terminal->bAssocTerminal = data[6];
        terminal->bSourceID = data[7];
        terminal->iTerminal = data[8];
        LOG_DEBUG("\t got OUTPUT terminal %d: type=%x", terminal->bTerminalID, terminal->wTerminalType);
        return terminal;
    }

    std::shared_ptr<uac_mixer_unit> parse_mixer_unit(const uint8_t *data, int size) {
        auto unit = std::make_shared<uac_mixer_unit>();
        unit->unitType = data[2];
        unit->bUnitID = data[3];
        return unit;
    }

    std::shared_ptr<uac_feature_unit> parse_feature_unit(const uint8_t *data, int size) {
        auto unit = std::make_shared<uac_feature_unit>();
        unit->unitType = data[2];
        unit->bUnitID = data[3];
        unit->bSourceId = data[4];
        unit->bControlSize = data[5];
        LOG_DEBUG("\t got FEATURE unit %d: bSourceId=%x", unit->bUnitID, unit->bSourceId);
        return unit;
    }

    uac_format_type_1* parse_as_format_type_1(const uint8_t *data, int size) {
        uac_format_type_1 *desc = new uac_format_type_1;
        desc->bFormatType = data[3];
        desc->bNrChannels = data[4];
        desc->bSubframeSize = data[5];
        desc->bBitResolution = data[6];
        desc->bSamFreqType = data[7];
        if (desc->bSamFreqType == 0) {
            desc->tLowerSamFreq = TO_DWORD24(data + 8);
            desc->tUpperSamFreq = TO_DWORD24(data + 11);
        } else {
            desc->tSamFreq = new uint32_t[desc->bSamFreqType];
            for (size_t i = 0; i < desc->bSamFreqType; ++i) {
                desc->tSamFreq[i] = TO_DWORD24(data + 8 + i*3);
                LOG_DEBUG("supported freq %d", desc->tSamFreq[i]);
            }
            
        }
        return desc;
    }

    void parse_as_general(uac_as_general &generalDesc, const uint8_t *data, int size) {
        generalDesc.bTerminalLink = data[3];
        generalDesc.bDelay = data[4];
        generalDesc.wFormatTag = TO_WORD(data+5);
    }

    void parse_as_format_type(uac_as_general &generalDesc, const uint8_t *data, int size) {
        uint8_t bFormatType = data[3];
        switch (bFormatType) {
        case UAC_FORMAT_TYPE_I:
            generalDesc.formatType = parse_as_format_type_1(data, size);
            break;
        
        default:
            break;
        }

    }

    void parse_as_intf(uac_stream_if &stream_if, const libusb_interface_descriptor *altsettings, int num_altsetting) {
        stream_if.streamSettings = new uac_as_general[num_altsetting-1];
        // skip altsetting 0, because it is non-configurable
        for (size_t i = 1; i < num_altsetting; ++i) {
            auto ifdesc = &altsettings[i];
            auto data = ifdesc->extra;
            int remaining = ifdesc->extra_length;
            while (remaining >= 3) {
                int descSize = data[0];
                auto subtype = data[2];
                switch (subtype) {
                case UAC_AS_GENERAL:
                    LOG_DEBUG("got AS_GENERAL descriptor");
                    parse_as_general(stream_if.streamSettings[i-1], data, descSize);
                    break;
                case UAC_AS_FORMAT_TYPE:
                    LOG_DEBUG("got AS_FORMAT_TYPE descriptor");
                    parse_as_format_type(stream_if.streamSettings[i-1], data, descSize);
                    break;
                case UAC_AS_FORMAT_SPECIFIC:
                    LOG_DEBUG("got AS_FORMAT_SPECIFIC descriptor");
                    break;
                default:
                    break;
                }
                remaining -= descSize;
                data += descSize;
            }
        }
        
    }


}
