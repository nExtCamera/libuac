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


#include "libuac.h"
#include <SDL.h>
#include <ctime>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

static SDL_AudioDeviceID audioDev;

std::shared_ptr<uac::uac_stream_handle> streamHandle;

const int TEX_WIDTH = 640;
const int TEX_HEIGHT = 480;

static void setupSDL() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    window = SDL_CreateWindow(
        "SDL2Test",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        TEX_WIDTH,
        TEX_HEIGHT,
        SDL_WINDOW_SHOWN);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, TEX_WIDTH, TEX_HEIGHT);


    SDL_AudioSpec audioSpec = {0,};
    audioSpec.freq = 48000,
    audioSpec.format = AUDIO_S16;
    audioSpec.channels = 2;
    audioSpec.samples = 512;
    audioSpec.callback = nullptr;
    audioSpec.userdata = nullptr;
    SDL_AudioSpec have;
    audioDev = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, &have, 0);
    if (audioDev == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to init Audio");
        exit(1);
    } else {
        SDL_PauseAudioDevice(audioDev, 0);
    }
}

static void cleanupSDL() {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static void eventLoop() {
    SDL_Event e;
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN) {
                quit = true;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                quit = true;
            }
        }
        //First clear the renderer
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        //Update the screen
        SDL_RenderPresent(renderer);
    }
}

static void cb(uint8_t *data, int size) {
    //SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "callback: %d", size);
    SDL_QueueAudio(audioDev, data, size);
}

int main() {
    setupSDL();
    std::shared_ptr<uac::uac_context> ctx;
    try {
        ctx = uac::uac_context::create();

        std::vector<std::shared_ptr<uac::uac_device>> devices = ctx->query_all_devices();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Available UAC devices: %ld", devices.size());

        if (devices.empty()) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "No UAC device available!");
            goto cleanup;
        }
        auto dev = devices[0];
        auto routes = dev->query_audio_routes(uac::UAC_TERMINAL_EXTERNAL_UNDEFINED, uac::UAC_TERMINAL_USB_STREAMING);
        if (routes.empty()) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "No USB streaming output!");
            goto cleanup;
        }

        auto& route = routes[0];

        auto& streamIfs = dev->get_stream_interface(route);
        // if (streamIfs.empty()) {
        //     SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "No streaming interfaces for selected Audio Function!");
        //     goto cleanup;
        // }
        
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Open device...");
        auto devHandle = dev->open();

        //SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Is Master Muted: %d",  devHandle->is_master_muted(route));
        //SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Master volume: %d",  devHandle->get_feature_master_volume(route));
        int setting = streamIfs.find_stream_setting(48000);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Start streaming...");
        streamHandle = devHandle->start_streaming(streamIfs, setting, &cb);

    } catch(const std::exception &e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "error: %s", e.what());
    }

    eventLoop();

    cleanup:
    streamHandle.reset();
    cleanupSDL();
}
