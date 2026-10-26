# lumex/tests/core/protocol/export/CMakeLists.txt

# LumexProtocolExportCxx<std>Tests: the Markdown snapshot of a layer with its
# registry and, with LUMEX_PROTOCOL_WITH_JSON, the JSON document checked
# against its structure (LumexProtocolExportJson.cxx11.tests.cpp is empty
# without the macro). The JSON header compiles against the consumer's own
# nlohmann/json; the tests use the vendored copy
# (cmake/LumexNlohmannJson.cmake).
set(_protocol_export_link lumex::protocol)
if(LUMEX_PROTOCOL_WITH_JSON)
    lumex_setup_nlohmann_json()
    list(APPEND _protocol_export_link nlohmann_json::nlohmann_json)
endif()

lumex_add_standard_suites(ProtocolExport
    MODULE protocol
    LINK ${_protocol_export_link}
    SOURCES ../LumexProtocolIndexTables.hpp
)
