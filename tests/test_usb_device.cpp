#include <doctest.h>
#include <libuac.h>

TEST_CASE("test uac_context::create()") {
    auto context = uac::uac_context::create();
}

TEST_CASE("test queryAllDevices()") {
    auto context = uac::uac_context::create();
    
    auto devices = context->queryAllDevices();
    CHECK(devices.size() >= 0);
}

TEST_CASE("test open()") {
    auto context = uac::uac_context::create();
    
    auto devices = context->queryAllDevices();
    REQUIRE(devices.size() >= 1);

    auto dev = devices[0];
    auto devHandle = dev->open();

    devHandle->close();
}
