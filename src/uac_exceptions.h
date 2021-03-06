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

#include <exception>
#include <string>
#include "libuac.h"

namespace uac {

    class uac_exception : public std::exception {
    public:
        uac_exception(const char* format, ...);

        virtual const char* what() const noexcept override;

    private:
        std::string message;
    };

    class invalid_device_exception : public uac_exception {
    public:
        invalid_device_exception() : uac_exception("Invalid device") {}
    };

    class usb_exception_impl : public usb_exception {
    public:
        usb_exception_impl(const std::string& msg, libusb_error error);

        virtual const char* what() const noexcept override;
        virtual libusb_error error_code() const override;

    private:
        std::string message;
        libusb_error errcode;
    };
}
