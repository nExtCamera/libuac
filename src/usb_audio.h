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

#include <stdint.h>

/* USB Device Class Definitions for Audio Devices 1.0 */
namespace uac {

    /**
     * Table A.2 Audio Interface Subclass Codes
     */
    enum uac_subclass_code {
        UAC_SUBCLASS_UNDEFINED      = 0x00,
        UAC_SUBCLASS_AUDIOCONTROL   = 0x01,
        UAC_SUBCLASS_AUDIOSTREAMING = 0x02,
        UAC_SUBCLASS_MIDISTREAMING  = 0x03
    };

    /**
     * Table A.3 Audio Interface Protocol Codes
     */
    enum uac_protocol_code {
        UAC_PROTOCOL_UNDEFINED = 0x00,
        // newer specs adds more values
    };

    /**
     * Table A.4 Audio Class-Specific Descriptor Types
     */
    enum uac_cs_descriptor_type {
        UAC_CS_UNDEFINED     = 0x20,
        UAC_CS_DEVICE        = 0x21,
        UAC_CS_CONFIGURATION = 0x22,
        UAC_CS_STRING        = 0x23,
        UAC_CS_INTERFACE     = 0x24,
        UAC_CS_ENDPOINT      = 0x25
    };

    /**
     * Table A.5 Audio Class-Specific AC Interface Descriptor Subtypes
     */
    enum uac_ac_descriptor_subtype {
        UAC_AC_DESCRIPTOR_UNDEFINED = 0x00,
        UAC_AC_HEADER = 0x01,
        UAC_AC_INPUT_TERMINAL = 0x02,
        UAC_AC_OUTPUT_TERMINAL = 0x03,
        UAC_AC_MIXER_UNIT = 0x04,
        UAC_AC_SELECTOR_UNIT = 0x05,
        UAC_AC_FEATURE_UNIT = 0x06,
        UAC_AC_PROCESSING_UNIT = 0x07,
        UAC_AC_EXTENSION_UNIT = 0x08
    };

    /**
     * Table A.6 Audio Class-Specific AS Interface Descriptor Subtypes
     */
    enum uac_as_descriptor_subtype {
        UAC_AS_DESCRIPTOR_UNDEFINED = 0x00,
        UAC_AS_GENERAL = 0x01,
        UAC_AS_FORMAT_TYPE = 0x02,
        UAC_AS_FORMAT_SPECIFIC = 0x03
    };

    /**
     * Frmts Table A.4 Format Type Codes
     */
    enum uac_format_type {
        UAC_FORMAT_TYPE_UNDEFINED = 0x00,
        UAC_FORMAT_TYPE_I = 0x01,
        UAC_FORMAT_TYPE_II = 0x02,
        UAC_FORMAT_TYPE_III = 0x03
    };

    struct uac_ac_header {
        uint16_t bcdADC;
        uint16_t wTotalLength;
    };

    struct uac_unit {
        uint8_t unitType;
        uint8_t bUnitID;
        std::vector<std::shared_ptr<uac_unit>> srcPins;

    };

    struct uac_input_terminal {
        uint8_t  bTerminalID;
        uint16_t wTerminalType;
        uint8_t  bAssocTerminal;
        uint8_t  bNrChannels;
        uint16_t wChannelConfig;
        uint8_t  iChannelNames;
        uint8_t  iTerminal;
    };

    struct uac_output_terminal {
        uint8_t  bTerminalID;
        uint16_t wTerminalType;
        uint8_t  bAssocTerminal;
        uint8_t  bSourceID;
        uint8_t  iTerminal;
    };

    struct uac_mixer_unit : uac_unit {
        /* data */
    };

    struct uac_feature_unit : uac_unit {
        uint8_t bSourceId;
        uint8_t bControlSize;
    };

    struct uac_format_type_desc {
        uint8_t bFormatType;
    };
    
    struct uac_as_general {
        uint8_t  bTerminalLink;
        uint8_t  bDelay;
        uint16_t wFormatTag;
        uac_format_type_desc *formatType;
    };

    struct uac_format_type_1 : uac_format_type_desc {
        uint8_t bNrChannels;
        uint8_t bSubframeSize;
        uint8_t bBitResolution;
        uint8_t bSamFreqType;

        // continous sampling freq
        uint32_t tLowerSamFreq;
        uint32_t tUpperSamFreq;

        // discreete sampling freq
        uint32_t* tSamFreq = nullptr;

        ~uac_format_type_1() {
            if (tSamFreq != nullptr) delete[] tSamFreq;
        }
    };

    struct uac_as_format_specific {
    };

}
