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
    enum uac_subclass_code : uint8_t {
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
    enum uac_ac_descriptor_subtype : uint8_t {
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
     * (Frmts) Table A.4 Format Type Codes
     */
    enum uac_format_type {
        UAC_FORMAT_TYPE_UNDEFINED = 0x00,
        UAC_FORMAT_TYPE_I = 0x01,
        UAC_FORMAT_TYPE_II = 0x02,
        UAC_FORMAT_TYPE_III = 0x03
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
        UAC_FORMAT_DATA_IEC1937_MPEG2_L2_LS = 0x2006
    };

    /**
     * Table 4-2: Class-Specific AC Interface Header Descriptor
     */
    struct uac_ac_header {
        uint16_t bcdADC;
        uint16_t wTotalLength;
        //uint8_t  bInCollection;
        //uint8_t  baInterfaceNr[];
    };

    /**
     * Table 4-3: Input Terminal Descriptor
     */
    struct uac_input_terminal {
        uint8_t  bTerminalID;
        uint16_t wTerminalType;
        uint8_t  bAssocTerminal;
        uint8_t  bNrChannels;
        uint16_t wChannelConfig;
        uint8_t  iChannelNames;
        uint8_t  iTerminal;
    };

    /**
     * Table 4-4: Output Terminal Descriptor
     */
    struct uac_output_terminal {
        uint8_t  bTerminalID;
        uint16_t wTerminalType;
        uint8_t  bAssocTerminal;
        uint8_t  bSourceID;
        uint8_t  iTerminal;
    };

    /**
     * Common fields for each unit descriptor
     *
     * See for example: Table 4-5: Mixer Unit Descriptor
     */
    struct uac_unit {
        uac_ac_descriptor_subtype unitType; // bDescriptorSubtype
        uint8_t bUnitID; // bUnitID
    };

    /**
     * Table 4-5: Mixer Unit Descriptor
     */
    struct uac_mixer_unit : uac_unit {
        /* data */
    };

    /**
     * Table 4-7: Feature Unit Descriptor
     */
    struct uac_feature_unit : uac_unit {
        uint8_t bSourceId;
        uint8_t bControlSize;
        uint8_t bmaControls[];
    };

    /*=============================
     *  Audio Format descriptors
     *=============================*/

    /**
     * A common format type descriptor data
     */
    struct uac_format_type_desc {
        uac_format_type bFormatType;
    };

    /*=============================
     *  Audio Streaming Descriptors
     *=============================*/

    /**
     * Table 4-19: Class-Specific AS Interface Descriptor
     */
    struct uac_as_general {
        uint8_t  bTerminalLink;
        uint8_t  bDelay;
        uac_audio_data_format_type wFormatTag;
    };

    /**
     * Table A-8: Audio Class-Specific Endpoint Descriptor Subtypes
     */
    enum uac_ep_desc_type {
        EP_GENERAL = 0x01
    };

    /**
     * Table 4-21: Class-Specific AS Isochronous Audio Data Endpoint Descriptor
     */
    struct iso_endpoint_desc {
        uint8_t bmAttributes;
        uint8_t bLockDelayUnits;
        uint16_t wLockDelay;
    };

    /**
     * (Frmts) Table 2-1: Type I Format Type Descriptor
     *
     * Identical structure is used for Type III Format Type Descriptor
     */
    struct uac_format_type_1 : uac_format_type_desc {
        uint8_t bNrChannels;
        uint8_t bSubframeSize;
        uint8_t bBitResolution;
        uint8_t bSamFreqType;

        // continuous sampling freq
        uint32_t tLowerSamFreq;
        uint32_t tUpperSamFreq;

        // discrete sampling freq
        uint32_t tSamFreq[];
    };

    /**
     * (Frmts) Table 2-4: Type II Format Type Descriptor
     */
    struct uac_format_type_2 : uac_format_type_desc {
        uint16_t wMaxBitRate;
        uint16_t wSamplesPerFrame;
        uint8_t bSamFreqType;

        // continuous sampling freq
        uint32_t tLowerSamFreq;
        uint32_t tUpperSamFreq;

        // discrete sampling freq
        uint32_t tSamFreq[];
    };

    /**
     * Table 2-7: MPEG Format-Specific Descriptor
     */
    struct uac_as_format_mpeg {
    };

    /**
     * Table 2-16: AC-3 Format-Specific Descriptor
     */
    struct uac_as_format_ac3 {
    };

    /*
     * USB standard request type
     */
    enum usb_request_type {
        REQ_TYPE_IF_SET = 0x21,
        REQ_TYPE_IF_GET = 0xA1,
        REQ_TYPE_EP_SET = 0x22,
        REQ_TYPE_EP_GET = 0xA2
    };

    /*
     * USB standard request get type
     */
    enum usb_request_get {
        REQ_SET_CUR = 0x01,
        REQ_SET_MIN = 0x02,
        REQ_SET_MAX = 0x03,
        REQ_SET_RES = 0x04,
        REQ_GET_CUR = 0x81,
        REQ_GET_MIN = 0x82,
        REQ_GET_MAX = 0x83,
        REQ_GET_RES = 0x84
    };

    /**
     * Table A-11: Feature Unit Control Selectors
     */
    enum uac_feature_unit_selectors {
        MUTE_CONTROL = 0x01,
        VOLUME_CONTROL = 0x02,
        BASS_CONTROL = 0x03,
        MID_CONTROL = 0x04,
        TREBLE_CONTROL = 0x05,
        GRAPHIC_EQUALIZER_CONTROL = 0x06,
        AUTOMATIC_GAIN_CONTROL = 0x07,
        DELAY_CONTROL = 0x08,
        BASS_BOOST_CONTROL = 0x09,
        LOUDNESS_CONTROL = 0x0A
    };

    /**
     * Table A-19: Endpoint Control Selectors
     */
    enum ep_control_selectors {
        SAMPLING_FREQ_CONTROL = 0x01,
        PITCH_CONTROL = 0x02
    };
}
