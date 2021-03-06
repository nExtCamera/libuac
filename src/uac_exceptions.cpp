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


#include "uac_exceptions.h"
#include <sstream>
#include <cstdarg>

namespace uac {

    uac_exception::uac_exception(const char* format, ...) {
        va_list args1;
        va_start(args1, format);
        va_list args2;
        va_copy(args2, args1);
        message.reserve(1+vsnprintf(nullptr, 0, format, args1));
        va_end(args1);
        vsnprintf(message.data(), message.capacity(), format, args2);
        va_end(args2);
    }

    const char* uac_exception::what() const noexcept {
        return message.c_str();
    }

    usb_exception_impl::usb_exception_impl(const std::string& msg, libusb_error error) : errcode(error) {
        std::stringstream ss;
        ss << msg << ' ' << libusb_error_name(errcode);
        message = ss.str();
    }

    libusb_error usb_exception_impl::error_code() const {
        return errcode;
    }

    const char* usb_exception_impl::what() const noexcept {
        return message.c_str();
    }
}