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
#include "uac_context.h"
#include "logging.h"
#include "uac_exceptions.h"

#define NUM_ISO_TRANSFERS 3

namespace uac {

    static void cb(libusb_transfer *transfer) {
        auto *strmh = static_cast<uac_stream_handle_impl*>(transfer->user_data);
        int errval;
        bool dropTransfer = false;
        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
                for (int packet_id = 0; packet_id < transfer->num_iso_packets; ++packet_id) {
                    libusb_iso_packet_descriptor* packet = transfer->iso_packet_desc + packet_id;
                    //LOG_DEBUG("packet %d actual_len=%d", packet_id, packet->actual_length);
                    if (packet->status == LIBUSB_TRANSFER_COMPLETED && packet->actual_length > 0) {
                        uint8_t *pktbuf = libusb_get_iso_packet_buffer(transfer, packet_id);
                        strmh->cb_func(pktbuf, packet->actual_length);
                    }
                }
                
            // fall through
            case LIBUSB_TRANSFER_TIMED_OUT:
                // resubmit transfer
                errval = libusb_submit_transfer(transfer);
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
            lock.unlock();
            strmh->mCv.notify_all();
        }
    }

    int uac_stream_if_impl::find_stream_setting(int32_t sampleRate) const {
        int idx = 0;
        for (auto&& setting : altsettings) {
            if (setting.supportsSampleRate(sampleRate)) return idx;
            else ++idx;
        }
        return -1;
    }

    int uac_stream_if_impl::get_bytes_per_transfer(uint8_t settingIdx) const {
        return altsettings[settingIdx].endpoint.wMaxPacketSize;
    }

    std::vector<uac_format> uac_stream_if_impl::getFormats() const {
        std::vector<uac_format> list;
        for (auto &&item : altsettings) {
            list.push_back(uac_format {
                .wFormatTag = item.general.wFormatTag,
                .pFormatDesc = item.general.format
            });
        }
        return list;
    }

    uac_stream_handle_impl::uac_stream_handle_impl(std::shared_ptr<uac_device_handle_impl> dev_handle, uint8_t interfaceNr, uint8_t altsetting, const uac_endpoint_desc &endpointDesc, const uac::uac_as_general& settings) :
        dev_handle(dev_handle), bInterfaceNr(interfaceNr), current_altsetting(altsetting), endpoint_desc(endpointDesc), settings(settings) {

        int errval;
        LOG_DEBUG("claim AS intf(%d)", bInterfaceNr);
        errval = libusb_claim_interface(dev_handle->usb_handle, bInterfaceNr);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("libusb_claim_interface()", (libusb_error)errval);
        }
        uac_format_type_1 *format = (uac_format_type_1*) settings.format.get();
        target_sampling_rate = format->tSamFreq[0];
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
        const int total_transfer_size = iso_packets * endpoint_desc.wMaxPacketSize;
        LOG_DEBUG("configure iso packets: wMaxPacketSize=%d, total_size=%d", endpoint_desc.wMaxPacketSize, total_transfer_size);
        auto bmAttributes = endpoint_desc.iso_desc.bmAttributes;
        if (bmAttributes & SAMPLING_FREQ_CONTROL) { // the endpoints supports sampling frequency, so probe it
            set_sampling_freq(target_sampling_rate);
        }

        LOG_DEBUG("set_altsetting %d at intf(%d) ep 0x%x", current_altsetting, bInterfaceNr, endpoint_desc.bEndpointAddress);
        int errval = libusb_set_interface_alt_setting(dev_handle->usb_handle, bInterfaceNr, current_altsetting);
        if (errval != LIBUSB_SUCCESS) {
            throw usb_exception_impl("libusb_set_interface_alt_setting()", (libusb_error)errval);
        }

        mActiveTransfers = NUM_ISO_TRANSFERS;
        for (int i = 0; i < NUM_ISO_TRANSFERS; ++i) {
            libusb_transfer* transfer = libusb_alloc_transfer(iso_packets);
            uint8_t *buffer = new uint8_t[total_transfer_size];
            memset(buffer, 0, total_transfer_size);

            libusb_fill_iso_transfer(transfer, dev_handle->usb_handle, endpoint_desc.bEndpointAddress, buffer, total_transfer_size, iso_packets, cb, this, 1000);
            libusb_set_iso_packet_lengths(transfer, endpoint_desc.wMaxPacketSize);
            errval = libusb_submit_transfer(transfer);
            LOG_DEBUG("submit transfer %d... %s", i, libusb_error_name(errval));
            if (errval == LIBUSB_SUCCESS) {
                transfers.push_back(transfer);
            } else {
                delete[] buffer;
                libusb_free_transfer(transfer);
                mActiveTransfers--;
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
        LOG_DEBUG("Stop stream intf(%d), altsetting=%d", bInterfaceNr, current_altsetting);
        for (libusb_transfer* transfer : transfers) {
            libusb_cancel_transfer(transfer);
        }
        
        libusb_set_interface_alt_setting(dev_handle->usb_handle, bInterfaceNr, 0);
        active = false;

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

    void uac_stream_handle_impl::setSamplingRate(const uint32_t samplingRate) {
        if (samplingRate == 0) {
            uac_format_type_1 *format = (uac_format_type_1*) settings.format.get();
            target_sampling_rate = format->tSamFreq[0];
        } else {
            target_sampling_rate = samplingRate;
        }
    }

    void uac_stream_handle_impl::set_sampling_freq(uint32_t sampling) {
        const int cs = SAMPLING_FREQ_CONTROL;
        const int ep = endpoint_desc.bEndpointAddress;
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
        const int ep = endpoint_desc.bEndpointAddress;
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

    uint8_t uac_format::getNumChannels() const {
        uac_format_type_1 *fmt;
        switch (pFormatDesc->bFormatType) {
            case UAC_FORMAT_TYPE_I:
                fmt = reinterpret_cast<uac_format_type_1*>(pFormatDesc.get());
                return fmt->bNrChannels;
            default:
                return 0;
        }
    }

    uint8_t uac_format::getSubframeSize() const {
        uac_format_type_1 *fmt;
        switch (pFormatDesc->bFormatType) {
            case UAC_FORMAT_TYPE_I:
                fmt = reinterpret_cast<uac_format_type_1*>(pFormatDesc.get());
                return fmt->bSubframeSize;
            default:
                return 0;
        }
    }

    uint8_t uac_format::getBitResolution() const {
        uac_format_type_1 *fmt;
        switch (pFormatDesc->bFormatType) {
            case UAC_FORMAT_TYPE_I:
                fmt = reinterpret_cast<uac_format_type_1*>(pFormatDesc.get());
                return fmt->bBitResolution;
            default:
                return 0;
        }
    }
}
