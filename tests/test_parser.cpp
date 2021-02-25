#include <doctest.h>
#include <libuac.h>


TEST_CASE("test parse()") {
    auto context = uac::uac_context::create();
    
    auto devices = context->queryAllDevices();
    REQUIRE(devices.size() >= 1);

    auto dev = devices[0];
}
