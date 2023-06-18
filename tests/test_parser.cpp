#include <doctest.h>
#include "libuac.h"
#include "uac_parser.h"
#include "usb_audio.h"

using namespace uac;

TEST_CASE("test parse_ac_header()") {
    uac_audiocontrol ac(1, 0);
    uint8_t hdr[] = { 0, 0, 0, /*bcdADC*/123, 0, /*wTotalLength*/10, 0, /*bInCollection*/0 };

    parse_ac_header(ac, hdr, sizeof(hdr));
    CHECK(ac.bcdADC == 123);
    CHECK(ac.wTotalLength == 10);
}

TEST_CASE("test building audio function topology") {
    uac_audiocontrol ac(1, 0);
    
    uac_output_terminal ot = uac_output_terminal {1, 0x100, 0, 3};
    uac_feature_unit ftunit = uac_feature_unit {UAC_AC_FEATURE_UNIT, 3, 2};
    uac_input_terminal it = uac_input_terminal {2, 0x200};
    
    ac.outputTerminals.push_back(std::make_shared<uac_output_terminal>(ot));
    ac.inputTerminals.push_back(std::make_shared<uac_input_terminal>(it));
    ac.units.push_back(std::make_shared<uac_feature_unit>(ftunit));

    ac.configure_audio_function();
    REQUIRE(ac.audio_routes().size() == 1);

    auto route = ac.audio_routes()[0];

    REQUIRE(route.entry->outTerminal->bTerminalID == ot.bTerminalID);

    ref_uac_audio_route ref_route = route;
    std::vector<ref_uac_audio_route> list;
    list.push_back(ref_route);
    const uac_audio_route& base_route = list[0];

    //const auto derivedTopology = dynamic_cast<const uac_audio_function_topology_impl&>(baseTopology);
}

TEST_CASE("test topology()") {
    uac_output_terminal ot = uac_output_terminal {1, UAC_TERMINAL_USB_STREAMING, 0, 2};
    uac_input_terminal it = uac_input_terminal {2, UAC_TERMINAL_MICROPHONE};

    REQUIRE_EQ(UAC_TERMINAL_USB_UNDEFINED & 0xFF , 0);
    REQUIRE_EQ(UAC_TERMINAL_USB_STREAMING & 0xFF , 1);

    auto entry = std::make_shared<uac_topology_entity>(std::make_shared<uac_output_terminal>(ot));
    entry->link_source(std::make_shared<uac_input_terminal>(it));
    uac_audio_route_impl topology(entry);

    CHECK(topology.contains_terminal(UAC_TERMINAL_USB_STREAMING) == true);
    CHECK(topology.contains_terminal(UAC_TERMINAL_USB_UNDEFINED) == true);
    CHECK(topology.contains_terminal(UAC_TERMINAL_MICROPHONE) == true);
    CHECK(topology.contains_terminal(UAC_TERMINAL_INPUT_UNDEFINED) == true);
}
