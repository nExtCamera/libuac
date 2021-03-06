#include <doctest.h>
#include <libuac.h>

TEST_CASE("test uac_context::create()") {
    auto context = uac::uac_context::create();
}

TEST_CASE("test query_all_devices()") {
    auto context = uac::uac_context::create();
    
    auto devices = context->query_all_devices();
    CHECK(devices.size() >= 0);
}
