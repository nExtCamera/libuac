# libuac

This is a C++ library implementing the USB Audio Class user-space driver.
The `libuac` was created for Android, but it should also work with any OS which is supported by [libusb](https://github.com/libusb/libusb).

# Features

* Implements USB Audio Class 1.0 (partially)
* Supports input audio devices only
* Workaround for some popular devices
* Built using C++17
* Thoroughly-tested in the [nExt Camera](https://play.google.com/store/apps/details?id=pl.nextcamera)

# Documentation

## Usage

A simple example of API usage:

```C++
#include <libuac.h>

auto context = uac::context::create(); // using a default libusb_context
auto devices = ctx->query_all_devices();
auto dev = devices[0];

auto routes = dev->query_audio_routes(uac::UAC_TERMINAL_EXTERNAL_UNDEFINED, uac::UAC_TERMINAL_USB_STREAMING);

auto& route = routes[0];

auto& streamIf = dev->get_stream_interface(route);
auto audio_config = streamIf.query_config_uncompressed(uac::UAC_FORMAT_DATA_PCM, 48000);

auto devHandle = dev->open();

streamHandle = devHandle->start_streaming(streamIf, *audio_config, &cb);

// stop streaming
streamHandle.reset()
```

For more details, see [sample code](./sample/example_sdl.cpp).

## Building



```sh
mkdir build && cd build
cmake ..
make
```

## Testing

The project uses CTest and [doctest](https://github.com/doctest/doctest) for unit testing.
To run tests, build project first and then execute:
```sh
ctest
```

# Licensing

Licensed under the Apache License, Version 2.0

[LICENSE](./LICENSE)
