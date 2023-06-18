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
#include "uac_context.h"
#include "uac_exceptions.h"
#include <list>
#include <sstream>
#include <utility>

namespace uac {

    class uac_config_desc {
        libusb_config_descriptor *config = nullptr;
    public:
        explicit uac_config_desc(libusb_device *udev) {
            int errval = libusb_get_active_config_descriptor(udev, &config);
            if (errval != LIBUSB_SUCCESS) {
                errval = libusb_get_config_descriptor(udev, 0, &config);
                if (errval != LIBUSB_SUCCESS) {
                    throw usb_exception_impl("libusb_get_config_descriptor()", (libusb_error) errval);
                }
            }
        }
        ~uac_config_desc() {
            libusb_free_config_descriptor(config);
        }
        libusb_config_descriptor* operator->() {
            return config;
        }
    };
    

    static std::unique_ptr<uac_audiocontrol> parse_audiocontrol(const libusb_interface_descriptor *ifdesc);
    
    static void scan_audiostreaming(uac_audiocontrol& ac, const libusb_interface *usbintf);
    static void parse_audiostreaming_intf(uac_stream_if_impl &stream_if, const libusb_interface_descriptor *altsettings, int num_altsetting);

    std::unique_ptr<uac_audiocontrol> uac_scan_device(libusb_device *udev) {
        uac_config_desc configDesc(udev);
        std::unique_ptr<uac_audiocontrol> audiocontrol;
        for (size_t i = 0; i < configDesc->bNumInterfaces; ++i) {
            auto intf_desc = configDesc->interface[i].altsetting;
            if (intf_desc->bInterfaceClass == LIBUSB_CLASS_AUDIO) {
                LOG_DEBUG("found AUDIO Class interface, subclass=0x%x, protocol=%d", intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol);
                switch (intf_desc->bInterfaceSubClass) {
                case (uint8_t)uac_subclass_code::UAC_SUBCLASS_AUDIOCONTROL:
                    audiocontrol = parse_audiocontrol(intf_desc);
                    break;
                case (uint8_t)uac_subclass_code::UAC_SUBCLASS_AUDIOSTREAMING:
                    if (!audiocontrol) {
                        // we expect the AudioControl interface before any AudioStreaming interfaces
                        throw invalid_device_exception();
                    }
                    scan_audiostreaming(*audiocontrol, &configDesc->interface[i]);
                    break;
                default:
                    break;
                }
            }
        }
        if (!audiocontrol) {
            // this is not a valid USB Audio Class device
            throw invalid_device_exception();
        }
        return audiocontrol;
    }

    std::unique_ptr<uac_audiocontrol> parse_audiocontrol(const libusb_interface_descriptor *ifdesc) {
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
            LOG_ERROR("expected a HEADER first but got an invalid descriptor sizeof(%d) %d:%d", descSize, descriptorType, subtype);
            return nullptr;
        }
        LOG_DEBUG("got HEADER descriptor. sizeof(%d)", descSize);
        auto audiocontrol = std::make_unique<uac_audiocontrol>(ifdesc->bInterfaceNumber, ifdesc->iInterface);
        parse_ac_header(*audiocontrol, data, descSize);

        if (audiocontrol->wTotalLength != remaining) {
            LOG_WARN("wTotalLength mismatch with actual data available: %d != %d", audiocontrol->wTotalLength, remaining);
        }
        
        remaining -= descSize;
        data += descSize;

        // parse other descriptors
        while (remaining >= 3) {
            descSize = data[0];
            descriptorType = data[1];
            subtype = data[2];
            if (remaining < descSize) {
                LOG_WARN("Bad descriptor size, exceeds remaining bytes %d < %d", remaining, descSize);
                break;
            }
            LOG_DEBUG("got descriptor sizeof(%d) %d:%d", descSize, descriptorType, subtype);
            switch (subtype) {
            case UAC_AC_HEADER:
                LOG_DEBUG("got another HEADER descriptor. A bug or buggy device?");
                break;
            case UAC_AC_INPUT_TERMINAL:
                audiocontrol->inputTerminals.push_back(parse_input_terminal(data, descSize));
                break;
            case UAC_AC_OUTPUT_TERMINAL:
                audiocontrol->outputTerminals.push_back(parse_output_terminal(data, descSize));
                break;
            case UAC_AC_MIXER_UNIT:
                audiocontrol->units.push_back(parse_mixer_unit(data, descSize));
                break;
            case UAC_AC_FEATURE_UNIT:
                audiocontrol->units.push_back(parse_feature_unit(data, descSize));
                break;
            
            default:
                LOG_DEBUG("Unsupported AC descriptor: %d, size=%d", subtype, descSize);
                break;
            }
            remaining -= descSize;
            data += descSize;
        }

        audiocontrol->configure_audio_function();
        return audiocontrol;
    }

    void scan_audiostreaming(uac_audiocontrol& ac, const libusb_interface *usbintf) {
        for (auto &&stream : ac.streams) {
            auto ifdesc = usbintf->altsetting;
            if (stream.bInterfaceNr == ifdesc->bInterfaceNumber) {
                LOG_DEBUG("parse AS interface %d", ifdesc->bInterfaceNumber);
                parse_audiostreaming_intf(stream, usbintf->altsetting, usbintf->num_altsetting);
                return;
            }
        }
        LOG_DEBUG("This AudioStreaming interface is not part of current AudioControl.");
    }

    void uac_audiocontrol::configure_audio_function() {
        for (auto &&terminal : outputTerminals) {
            auto route = build_audio_topology(terminal);
            audioFunctionTopology.push_back(route);
        }
    }

    uac_audio_route_impl uac_audiocontrol::build_audio_topology(std::shared_ptr<uac_output_terminal> outputTerminal) {
        std::stringstream logStream;
        auto outputEntity = std::make_shared<uac_topology_entity>(outputTerminal);

        std::list<uac_topology_entity*> entities;
        std::string verbose;
        entities.push_back(outputEntity.get());
        logStream << "out " << (int) outputTerminal->bTerminalID;
        while (!entities.empty()) {
            auto entity = entities.front();
            entities.pop_front();
            for (auto sourceId : entity->source_ids()) {
                // check unit first
                auto unit = find_unit(sourceId);
                if (unit != nullptr) {
                    logStream << " < unit " << (int) unit->bUnitID;
                    entity = entity->link_source(unit);
                    entities.push_back(entity);
                } else {
                    // then maybe it's an input terminal
                    auto inTerminal = find_input_terminal(sourceId);
                    if (inTerminal != nullptr) {
                        logStream << " < in " << (int) inTerminal->bTerminalID;
                        entity = entity->link_source(inTerminal);
                    } else {
                        logStream << " <- This topology looks invalid, not ending with the Terminal.";
                    }
                }
            }
        }
        LOG_DEBUG("audio route chain : %s", logStream.str().c_str());
        return {outputEntity};
    }

    std::shared_ptr<uac_unit> uac_audiocontrol::find_unit(int id) {
        for (auto &&unit : units) {
            if (unit->bUnitID == id) return unit;
        }
        return {}; // empty
    }

    std::shared_ptr<uac_input_terminal> uac_audiocontrol::find_input_terminal(int id) {
        for (auto &&terminal : inputTerminals) {
            if (terminal->bTerminalID == id) return terminal;
        }
        return {}; // empty
    }

    uac_topology_entity::uac_topology_entity(std::shared_ptr<uac_unit> unit) : unit(std::move(unit)) {}

    uac_topology_entity::uac_topology_entity(std::shared_ptr<uac_input_terminal> inTerminal) : inTerminal(std::move(inTerminal)) {}

    uac_topology_entity::uac_topology_entity(std::shared_ptr<uac_output_terminal> outTerminal) : outTerminal(std::move(outTerminal)) {}

    uac_topology_entity::~uac_topology_entity() {
        for (auto &&i : sources) {
            delete i;
        }
    }

    std::vector<int> uac_topology_entity::source_ids() const {
        auto ids = std::vector<int>();
        if (outTerminal != nullptr) {
            ids.push_back(outTerminal->bSourceID);
        } else if (unit != nullptr) {
            if (unit->unitType == UAC_AC_FEATURE_UNIT) {
                uac_feature_unit *ftunit = static_cast<uac_feature_unit*>(unit.get());
                ids.push_back(ftunit->bSourceId);
            }
        }
        return ids;
    }

    uac_topology_entity* uac_topology_entity::link_source(std::shared_ptr<uac_unit> srcUnit) {
        auto entity = new uac_topology_entity(std::move(srcUnit));
        entity->sink = this;
        sources.push_back(entity);
        return entity;
    }

    uac_topology_entity* uac_topology_entity::link_source(std::shared_ptr<uac_input_terminal> terminal) {
        auto entity = new uac_topology_entity(std::move(terminal));
        entity->sink = this;
        sources.push_back(entity);
        return entity;
    }

    void parse_ac_header(uac_audiocontrol& ac, const uint8_t *data, int size) {
        ac.bcdADC = TO_WORD(data+3);
        ac.wTotalLength = TO_WORD(data+5);
        uint8_t bInCollection = data[7];
        for (size_t i = 0; i < bInCollection; ++i) {
            ac.streams.emplace_back(data[8+i]);
            LOG_DEBUG("\t got Audio Streaming interface at: %d", ac.streams.back().bInterfaceNr);
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
        LOG_DEBUG("\t got INPUT_TERMINAL %d: type=0x%x", terminal->bTerminalID, terminal->wTerminalType);
        return terminal;
    }

    std::shared_ptr<uac_output_terminal> parse_output_terminal(const uint8_t *data, int size) {
        auto terminal = std::make_shared<uac_output_terminal>();
        terminal->bTerminalID = data[3];
        terminal->wTerminalType = TO_WORD(data+4);
        terminal->bAssocTerminal = data[6];
        terminal->bSourceID = data[7];
        terminal->iTerminal = data[8];
        LOG_DEBUG("\t got OUTPUT_TERMINAL %d: type=0x%x", terminal->bTerminalID, terminal->wTerminalType);
        return terminal;
    }

    std::shared_ptr<uac_mixer_unit> parse_mixer_unit(const uint8_t *data, int size) {
        auto unit = std::make_shared<uac_mixer_unit>();
        unit->unitType = data[2];
        unit->bUnitID = data[3];
        return unit;
    }

    std::shared_ptr<uac_feature_unit> parse_feature_unit(const uint8_t *data, int size) {
        auto unit = std::make_unique<uac_feature_unit>();
        unit->unitType = data[2];
        unit->bUnitID = data[3];
        unit->bSourceId = data[4];
        unit->bControlSize = data[5];
        LOG_DEBUG("\t got FEATURE_UNIT %d: bSourceId=0x%x", unit->bUnitID, unit->bSourceId);
        return unit;
    }

    uac_format_type_1* parse_as_format_type_1(const uint8_t *data, int size) {
        uint8_t bSamFreqType = data[7];
        uac_format_type_1 *desc = (uac_format_type_1*) malloc(sizeof(uac_format_type_1) + sizeof(uint32_t [bSamFreqType]));
        desc->bFormatType = (uac_format_type) data[3];
        desc->bNrChannels = data[4];
        desc->bSubframeSize = data[5];
        desc->bBitResolution = data[6];
        desc->bSamFreqType = bSamFreqType;
        if (desc->bSamFreqType == 0) {
            desc->tLowerSamFreq = TO_DWORD24(data + 8);
            desc->tUpperSamFreq = TO_DWORD24(data + 11);
        } else {
            desc->tLowerSamFreq = 0;
            desc->tUpperSamFreq = 0;
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
        generalDesc.wFormatTag = (uac_audio_data_format_type) TO_WORD(data+5);
    }

    void parse_as_format_type(uac_as_general &generalDesc, const uint8_t *data, int size) {
        uint8_t bFormatType = data[3];
        switch (bFormatType) {
        case UAC_FORMAT_TYPE_I:
            generalDesc.format = std::shared_ptr<uac_format_type_desc>(parse_as_format_type_1(data, size));
            break;
        
        default:
            generalDesc.format = std::shared_ptr<uac_format_type_desc>((uac_format_type_desc*)malloc(sizeof(uac_format_type_desc)));
            generalDesc.format->bFormatType = static_cast<uac_format_type>(bFormatType);
            break;
        }

    }

    void parse_iso_ep(iso_endpoint_desc& desc, const uint8_t *data, int size) {
        int remaining = size;
        while (remaining > 3) {
            int length = data[0];
            remaining -= length;
            if (data[2] == EP_GENERAL) {
                desc.bmAttributes = data[3];
                desc.bLockDelayUnits = data[4];
                desc.wLockDelay = TO_WORD(data + 5);
            }
        }
        
    }

    void parse_audiostreaming_intf(uac_stream_if_impl &stream_if, const libusb_interface_descriptor *altsettings, int num_altsetting) {
        // skip altsetting 0, because it is non-configurable
        for (size_t i = 1; i < num_altsetting; ++i) {
            auto ifdesc = &altsettings[i];
            LOG_DEBUG("parsing altsetting=%d descriptor...", ifdesc->bAlternateSetting);
            auto& altsetting = stream_if.altsettings.emplace_back();
            auto data = ifdesc->extra;
            int remaining = ifdesc->extra_length;

            altsetting.bAlternateSetting = ifdesc->bAlternateSetting;

            bool hasGeneralDescriptor = false;
            bool hasFormatDescriptor = false;
            while (remaining >= 3) {
                int descSize = data[0];
                auto subtype = data[2];
                switch (subtype) {
                case UAC_AS_GENERAL:
                    LOG_DEBUG("got AS_GENERAL descriptor");
                    parse_as_general(altsetting.general, data, descSize);
                    hasGeneralDescriptor = true;
                    break;
                case UAC_AS_FORMAT_TYPE:
                    LOG_DEBUG("got AS_FORMAT_TYPE descriptor");
                    parse_as_format_type(altsetting.general, data, descSize);
                    hasFormatDescriptor = true;
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

            if (!hasGeneralDescriptor || !hasFormatDescriptor || ifdesc->bNumEndpoints == 0) {
                stream_if.altsettings.pop_back();
                continue;
            }
            
            if (ifdesc->bNumEndpoints != 1) {
                stream_if.altsettings.pop_back();
                LOG_ERROR("Invalid number of endpoints in this interface(%d): %d", i, ifdesc->bNumEndpoints);
            } else {
                LOG_DEBUG("altsetting endpointAddress=%x, wMaxPacketSize=%d", ifdesc->endpoint->bEndpointAddress, ifdesc->endpoint->wMaxPacketSize);
                auto& epDesc = altsetting.endpoint;
                epDesc.bEndpointAddress = ifdesc->endpoint->bEndpointAddress;
                epDesc.wMaxPacketSize = ifdesc->endpoint->wMaxPacketSize;
                if ((ifdesc->endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
                    parse_iso_ep(epDesc.iso_desc, ifdesc->endpoint->extra, ifdesc->endpoint->extra_length);
                } else {
                    LOG_DEBUG("Unsupported transfer type.");
                    stream_if.altsettings.pop_back();
                }
            }
        }
        
    }

    static bool matches_terminals(uint16_t terminalType, uac_terminal_type expected) {
        if ((expected & 0xFF) == 0) {
            auto mask = expected | 0xFF;
            return (terminalType & mask) == terminalType;
        } else {
            return terminalType == expected;
        }
    }

    uac_audio_route_impl::uac_audio_route_impl(std::shared_ptr<uac_topology_entity> entry) : entry(entry) {
        LOG_DEBUG("construct uac_audio_route_impl %p", this);
    }
    
    uac_topology_entity* uac_audio_route_impl::findInputTerminalByType(uac_topology_entity *entity, uac_terminal_type terminalType) {
        if (entity->inTerminal != nullptr && matches_terminals(entity->inTerminal->wTerminalType, terminalType)) {
            return entity;
        } else {
            for (auto &&e : entity->sources) {
                auto other = findInputTerminalByType(e, terminalType);
                if (other != nullptr) {
                    return other;
                }
            }
            return nullptr;
        }
    }

    bool uac_audio_route_impl::contains_terminal(uac_terminal_type terminalType) const {
        if (matches_terminals(entry->outTerminal->wTerminalType, terminalType)) {
            return true;
        } else {
            return findInputTerminalByType(entry.get(), terminalType) != nullptr;
        }
    }

    bool uac_altsetting::supportsSampleRate(int32_t sampleRate) const {
        auto format = general.format;
        bool result = false;
        uac_format_type_1 *format1;
        switch (format->bFormatType) {
            case UAC_FORMAT_TYPE_I:
                format1 = static_cast<uac_format_type_1 *>(format.get());
                if (format1->bSamFreqType == 0) {
                    result = format1->tLowerSamFreq <= sampleRate &&
                    sampleRate <= format1->tUpperSamFreq;
                } else {
                    for (int i = 0; i < format1->bSamFreqType; ++i) {
                        if (format1->tSamFreq[i] == sampleRate) {
                            result = true;
                            break;
                        }
                    }
                }
                break;
            default:
                break;
        }
        return result;
    }
}
