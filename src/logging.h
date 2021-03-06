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

#include <cstdio>
#include <cstring>
#include <config.h>

#ifdef __ANDROID__
#include <android/log.h>

#ifdef UAC_ENABLE_LOGGING
#define LOG_DEBUG(format, ...) __android_log_print(ANDROID_LOG_DEBUG, "UAC", "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN(format, ...) __android_log_print(ANDROID_LOG_WARN, "UAC", "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ENTER() __android_log_print(ANDROID_LOG_VERBOSE, "UAC", "[%s:%d] begin %s\n", basename(__FILE__), __LINE__, __FUNCTION__)
#define LOG_EXIT(code) __android_log_print(ANDROID_LOG_VERBOSE, "UAC", "[%s:%d] end %s (%d)\n", basename(__FILE__), __LINE__, __FUNCTION__, code)
#define LOG_EXIT_VOID() __android_log_print(ANDROID_LOG_VERBOSE, "UAC", "[%s:%d] end %s\n", basename(__FILE__), __LINE__, __FUNCTION__)
#else
#define LOG_DEBUG(...)
#define LOG_WARN(format, ...)
#define LOG_ENTER()
#define LOG_EXIT(code)
#define LOG_EXIT_VOID()
#endif //UAC_ENABLE_LOGGING

#ifdef THROW_ON_ERROR
#define LOG_ERROR(format, ...) throw uac::uac_exception(format, ##__VA_ARGS__)
#else
#define LOG_ERROR(format, ...) __android_log_print(ANDROID_LOG_ERROR, "UAC", "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif //THROW_ON_ERROR
#else

#define LOG_DEBUG(format, ...) fprintf(stderr, "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN(format, ...) fprintf(stderr, "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ENTER() fprintf(stderr, "[%s:%d] begin %s\n", basename(__FILE__), __LINE__, __FUNCTION__)
#define LOG_EXIT(code) fprintf(stderr, "[%s:%d] end %s (%d)\n", basename(__FILE__), __LINE__, __FUNCTION__, code)
#define LOG_EXIT_VOID() fprintf(stderr, "[%s:%d] end %s\n", basename(__FILE__), __LINE__, __FUNCTION__)

#ifdef THROW_ON_ERROR
#define LOG_ERROR(format, ...) throw uac::uac_exception(format, ##__VA_ARGS__)
#else
#define LOG_ERROR(format, ...) fprintf(stderr, "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif //THROW_ON_ERROR

#endif //__ANDROID__

#ifdef VERBOSE_VLOG
    #define LOG_VERBOSE(...) LOG_DEBUG(__VA_ARGS__)
#else
    #define LOG_VERBOSE(...)
#endif
