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

#include "uac_streaming.h"

#include <utility>
#include <set>
#include "uac_context.h"
#include "logging.h"
#include "uac_exceptions.h"

#define NUM_ISO_TRANSFERS 8

namespace uac {

    void uac_stream_handle_impl::cb(libusb_transfer *transfer) {
        auto *strmh = static_cast<uac_stream_handle_impl*>(transfer->user_data);
        int errval;
        bool dropTransfer = false;
        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
                for (int packet_id = 0; packet_id < transfer->num_iso_packets; ++packet_id) {
                    libusb_iso_packet_descriptor* packet = transfer->iso_packet_desc + packet_id;
                    //LOG_DEBUG("packet %d actual_len=%u", packet_id, packet->actual_length);
                    if (packet->actual_length > packet->length) {
                        LOG_WARN("kernel misbehaviour with returned actual_length (%u>%u)", packet->actual_length, packet->length);
                        strmh->usbTransferError = UAC_ERROR_KERNEL_MALFUNCTION;
                        dropTransfer = true;
                        break;
                    }
                    if (packet->status == LIBUSB_TRANSFER_COMPLETED && packet->actual_length > 0) {
                        uint8_t *pktbuf = libusb_get_iso_packet_buffer(transfer, packet_id);
                        if (strmh->offset_stream > 0) {
                            uint offset = std::min(strmh->offset_stream, packet->actual_length);
                            pktbuf += offset;
                            packet->actual_length -= offset;
                            strmh->offset_stream -= offset;
                            LOG_DEBUG("SWAP CHANNELS packet %d actual_len=%d offset=%d", packet_id, packet->actual_length, offset);
                        }
                        strmh->cb_func(pktbuf, packet->actual_length);
                    }
                }
                if (dropTransfer) break;
                // else, fall through
            case LIBUSB_TRANSFER_TIMED_OUT:
                // resubmit transfer
                errval = strmh->active ? libusb_submit_transfer(transfer) : LIBUSB_ERROR_INTERRUPTED;
                if (errval != LIBUSB_SUCCESS) {
                    LOG_DEBUG("on time out: submit transfer... %s", libusb_error_name(errval));
                    dropTransfer = true;
                }
                break;
	        case LIBUSB_TRANSFER_ERROR:
            case LIBUSB_TRANSFER_CANCELLED:
            case LIBUSB_TRANSFER_STALL:
	        case LIBUSB_TRANSFER_NO_DEVICE:
	        case LIBUSB_TRANSFER_OVERFLOW:
                LOG_WARN("finish transfer due to %s", libusb_error_name(transfer->status));
                dropTransfer = true;
                break;
        }
        if (dropTransfer) {
            LOG_DEBUG("drop transfer... %d", strmh->mActiveTransfers);
            std::unique_lock lock(strmh->mMutex);
            strmh->mActiveTransfers--;
            if (strmh->is_active()) {
                strmh->usbTransferError = UAC_ERROR_TRANSFERS_WITHERED;
            }
            lock.unlock();
            strmh->mCv.notify_all();
        }
    }

    std::vector<uac_audio_data_format_type> uac_stream_if_impl::get_audio_formats() const {
        std::set<uac_audio_data_format_type> formats;
        for (auto &&item : altsettings) {
            formats.insert(static_cast<uac_audio_data_format_type>(item.general.wFormatTag));
        }
        return {formats.begin(), formats.end()};
    }

    std::vector<uint8_t> uac_stream_if_impl::get_channel_counts(uac_audio_data_format_type fmt) const {
        std::set<uint8_t> channels;
        for (auto &&item : altsettings) {
            if (item.general.wFormatTag != fmt) continue;
            const uac_format_type_1* formatType1;
            switch (item.formatTypeDesc->bFormatType) {
                case UAC_FORMAT_TYPE_I:
                case UAC_FORMAT_TYPE_III:
                    formatType1 = reinterpret_cast<const uac_format_type_1*>(item.formatTypeDesc.get());
                    channels.insert(formatType1->bNrChannels);
                    break;
                default:
                    break;
            }
        }
        return {channels.begin(), channels.end()};
    }

    std::vector<uint8_t> uac_stream_if_impl::get_bit_resolutions(uac_audio_data_format_type fmt) const {
        std::set<uint8_t> bitres;
        for (auto &&item : altsettings) {
            if (item.general.wFormatTag != fmt) continue;
            const uac_format_type_1* formatType1;
            switch (item.formatTypeDesc->bFormatType) {
                case UAC_FORMAT_TYPE_I:
                case UAC_FORMAT_TYPE_III:
                    formatType1 = reinterpret_cast<const uac_format_type_1*>(item.formatTypeDesc.get());
                    bitres.insert(formatType1->bBitResolution);
                    break;
                default:
                    break;
            }
        }
        return {bitres.begin(), bitres.end()};
    }

    std::vector<uint32_t> uac_stream_if_impl::get_sample_rates(uac_audio_data_format_type fmt) const {
        std::set<uint32_t> samplingRates;
        for (auto &&item : altsettings) {
            if (item.general.wFormatTag != fmt) continue;
            const uac_format_type_1* formatType1;
            switch (item.formatTypeDesc->bFormatType) {
                case UAC_FORMAT_TYPE_I:
                case UAC_FORMAT_TYPE_III:
                    formatType1 = reinterpret_cast<const uac_format_type_1*>(item.formatTypeDesc.get());
                    if (formatType1->bSamFreqType > 0) {
                        for (int i = 0; i < formatType1->bSamFreqType; ++i) {
                            samplingRates.insert(formatType1->tSamFreq[i]);
                        }
                    } else {
                        samplingRates.insert(formatType1->tLowerSamFreq);
                        samplingRates.insert(formatType1->tUpperSamFreq);
                    }
                    break;
                default:
                    break;
            }
        }
        return { samplingRates.begin(), samplingRates.end() };
    }

    std::unique_ptr<const uac_audio_config_uncompressed> uac_stream_if_impl::query_config_uncompressed(
            uac_audio_data_format_type audioDataFormatType,
            uint8_t numChannels,
            uint32_t sampleRate) const {
        for (auto&& setting : altsettings) {
            auto format1 = setting.getFormatType1();
            if (format1 == nullptr) continue;
            if ((audioDataFormatType == UAC_FORMAT_DATA_ANY || setting.general.wFormatTag == audioDataFormatType)
                && setting.supportsChannelsCount(numChannels)
                && setting.supportsSampleRate(sampleRate)) {
                return std::make_unique<uac_audio_config_uncompressed>(
                        uac_audio_config_uncompressed{
                            audioDataFormatType,
                            setting.bAlternateSetting,
                            format1->bSubframeSize,
                            format1->bBitResolution,
                            format1->bNrChannels,
                            setting.endpoint.wMaxPacketSize,
                            sampleRate
                            });
            }
        }
        return {nullptr};
    }

    uac_stream_handle_impl::uac_stream_handle_impl(const std::shared_ptr<uac_device_handle_impl>& dev_handle, uint8_t interfaceNr, const uac_altsetting& altsetting) :
        dev_handle(dev_handle), bInterfaceNr(interfaceNr), altsetting(altsetting), mActiveTransfers(0) {

        int errval;
        LOG_DEBUG("claim AS intf(%d)", bInterfaceNr);
        errval = libusb_claim_interface(dev_handle->usb_handle, bInterfaceNr);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("libusb_claim_interface()", (libusb_error)errval);
        }
        uac_format_type_1 *format = (uac_format_type_1*) altsetting.formatTypeDesc.get();
        target_sampling_rate = format->tSamFreq[0];
        stride = format->bSubframeSize * format->bNrChannels;

        if (dev_handle->device->hasQuirkSwapChannels()) {
            offset_stream = format->bSubframeSize;
        } else {
            offset_stream = 0;
        }
    }

    uac_stream_handle_impl::~uac_stream_handle_impl() {
        stop();
        LOG_DEBUG("Destroy stream handle and release intf(%d)", bInterfaceNr);
        auto errval = libusb_release_interface(dev_handle->usb_handle, bInterfaceNr);
        if (errval != LIBUSB_SUCCESS) {
            LOG_DEBUG("Got error when releasing a stream: %s", libusb_error_name(errval));
        }
    }

    void uac_stream_handle_impl::start(stream_cb_func stream_cb_func, int burst) {
        this->cb_func = std::move(stream_cb_func);
        const int iso_packets = burst;
        const uint16_t wMaxPacketSize = altsetting.endpoint.wMaxPacketSize;
        const int transfer_size = iso_packets * wMaxPacketSize;
        LOG_DEBUG("configure iso packets: wMaxPacketSize=%d, transfer_size=%d", wMaxPacketSize, transfer_size);
        auto bmAttributes = altsetting.endpoint.iso_desc.bmAttributes;
        if (bmAttributes & SAMPLING_FREQ_CONTROL) { // the endpoints supports sampling frequency, so probe it
            set_sampling_freq(target_sampling_rate);
        }

        LOG_DEBUG("set_altsetting %d at intf(%d) ep 0x%x", altsetting.bAlternateSetting, bInterfaceNr, altsetting.endpoint.bEndpointAddress);
        int errval = libusb_set_interface_alt_setting(dev_handle->usb_handle, bInterfaceNr, altsetting.bAlternateSetting);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("libusb_set_interface_alt_setting()", (libusb_error)errval);
        }

        mActiveTransfers = 0;
        for (int i = 0; i < NUM_ISO_TRANSFERS; ++i) {
            libusb_transfer* transfer = libusb_alloc_transfer(iso_packets);
            if (transfer == nullptr) {
                break;
            }
            uint8_t *buffer = new (std::nothrow) uint8_t[transfer_size];
            if (buffer == nullptr) {
                libusb_free_transfer(transfer);
                break;
            }
            memset(buffer, 0, transfer_size);

            libusb_fill_iso_transfer(transfer, dev_handle->usb_handle, altsetting.endpoint.bEndpointAddress, buffer, transfer_size, iso_packets, cb, this, 1000);
            libusb_set_iso_packet_lengths(transfer, wMaxPacketSize);
            errval = libusb_submit_transfer(transfer);
            LOG_DEBUG("submit transfer %d... %s", i, libusb_error_name(errval));
            if (errval == LIBUSB_SUCCESS) {
                transfers.push_back(transfer);
                ++mActiveTransfers;
            } else {
                libusb_free_transfer(transfer);
                delete[] buffer;
            }
        }

        if (transfers.empty()) {
            libusb_set_interface_alt_setting(dev_handle->usb_handle, bInterfaceNr, 0);
            throw std::runtime_error("No transfers submitted!");
        }
        

        active = true;
    }

    void uac_stream_handle_impl::stop() {
        if (!active) return;
        active = false;
        LOG_DEBUG("Stop stream intf(%d), altsetting=%d", bInterfaceNr, altsetting.bAlternateSetting);
        active = false;
        for (libusb_transfer* transfer : transfers) {
            libusb_cancel_transfer(transfer);
        }
        
        libusb_set_interface_alt_setting(dev_handle->usb_handle, bInterfaceNr, 0);

        if (transfers.empty()) {
            return;
        }

        // wait for transfers to complete
        std::unique_lock lock(mMutex);
        mCv.wait(lock, [this] { return mActiveTransfers == 0; });

        // free transfers
        LOG_DEBUG("Free up transfers..");
        for (libusb_transfer* transfer : transfers) {
            delete[] transfer->buffer;
            libusb_free_transfer(transfer);
        }
        transfers.clear();
    }

    void uac_stream_handle_impl::set_sampling_rate(const uint32_t samplingRate) {
        if (samplingRate == 0) {
            uac_format_type_1 *format = (uac_format_type_1*) altsetting.formatTypeDesc.get();
            target_sampling_rate = format->tSamFreq[0];
        } else {
            target_sampling_rate = samplingRate;
        }
    }

    void uac_stream_handle_impl::set_sampling_freq(uint32_t sampling) {
        const int cs = SAMPLING_FREQ_CONTROL;
        const int ep = altsetting.endpoint.bEndpointAddress;
        uint8_t data[3] = H_DWORD24(sampling);

        LOG_DEBUG("set_sampling_freq (%d)", sampling);
        int errval = libusb_control_transfer(
            dev_handle->usb_handle,
            REQ_TYPE_EP_SET,
            REQ_SET_CUR,
            cs << 8,
            ep,
            (uint8_t*) &data,
            sizeof(data),
            0 /* timeout */);

        if (errval < 0)
            throw usb_exception_impl("set_sampling_freq()", (libusb_error)errval);
    }

    uint32_t uac_stream_handle_impl::get_sampling_freq() {
        const int cs = SAMPLING_FREQ_CONTROL;
        const int ep = altsetting.endpoint.bEndpointAddress;
        uint8_t data[3];

        int errval = libusb_control_transfer(
            dev_handle->usb_handle,
            REQ_TYPE_EP_GET,
            REQ_GET_CUR,
            cs << 8,
            ep,
            (uint8_t *)&data,
            sizeof(data),
            0 /* timeout */);

        if (errval < 0)
            throw usb_exception_impl("get_sampling_freq()", (libusb_error)errval);

        uint32_t samplingFreq = TO_DWORD24(data);
        LOG_DEBUG("get_sampling_freq (%d)", samplingFreq);
        return samplingFreq;
    }

    error_code uac_stream_handle_impl::check_streaming_error() const {
        return usbTransferError;
    }

    bool uac_stream_handle_impl::is_active() const {
        return active;
    }
}
