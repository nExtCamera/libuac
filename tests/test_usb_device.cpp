#include <doctest.h>
#include "libuac.h"
#include "usb_audio.h"
#include "uac_parser.h"

using namespace uac;

TEST_CASE("test open()") {
    auto context = uac_context::create();
    
    auto devices = context->query_all_devices();
    REQUIRE(devices.size() >= 1);

    auto dev = devices[0];
    auto devHandle = dev->open();

    devHandle->dump(nullptr);

    devHandle->close();
}


TEST_CASE("test query_audio_function()") {
    auto context = uac_context::create();
    
    auto devices = context->query_all_devices();
    REQUIRE(devices.size() >= 1);

    auto dev = devices[0];

    const auto routes = dev->query_audio_routes(UAC_TERMINAL_MICROPHONE, UAC_TERMINAL_USB_STREAMING);

    REQUIRE(routes.size() > 0);

    auto& streamIfs = dev->get_stream_interface(routes[0]);

    auto devHandle = dev->open();
    CHECK(devHandle->is_master_muted(routes[0]) == false);
    
    CHECK(devHandle->get_feature_master_volume(routes[0]) > 0);
}
