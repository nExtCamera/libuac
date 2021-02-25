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
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#define UVC_DEBUG(format, ...) __android_log_print(ANDROID_LOG_DEBUG, "UVC", "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define UVC_ERROR(format, ...) __android_log_print(ANDROID_LOG_ERROR, "UVC", "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define UVC_ENTER() __android_log_print(ANDROID_LOG_VERBOSE, "UVC", "[%s:%d] begin %s\n", basename(__FILE__), __LINE__, __FUNCTION__)
#define UVC_EXIT(code) __android_log_print(ANDROID_LOG_VERBOSE, "UVC", "[%s:%d] end %s (%d)\n", basename(__FILE__), __LINE__, __FUNCTION__, code)
#define UVC_EXIT_VOID() __android_log_print(ANDROID_LOG_VERBOSE, "UVC", "[%s:%d] end %s\n", basename(__FILE__), __LINE__, __FUNCTION__)

#else

#define LOG_DEBUG(format, ...) fprintf(stderr, "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) fprintf(stderr, "[%s:%d/%s] " format "\n", basename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ENTER() fprintf(stderr, "[%s:%d] begin %s\n", basename(__FILE__), __LINE__, __FUNCTION__)
#define LOG_EXIT(code) fprintf(stderr, "[%s:%d] end %s (%d)\n", basename(__FILE__), __LINE__, __FUNCTION__, code)
#define LOG_EXIT_VOID() fprintf(stderr, "[%s:%d] end %s\n", basename(__FILE__), __LINE__, __FUNCTION__)
#endif

#ifdef UVC_VLOG
    #define UVC_VERBOSE(...) UVC_DEBUG(__VA_ARGS__)
#else
    #define UVC_VERBOSE(...)
#endif
