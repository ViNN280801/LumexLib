# examples/CMakeLists.txt

set(EX1_NAME LumexXmlExample1)
add_executable(${EX1_NAME} xml/example_1.cpp)
target_link_libraries(${EX1_NAME}
    PRIVATE
    LumexXml
)

set(EX2_NAME LumexXmlExample2)
add_executable(${EX2_NAME} xml/example_2.cpp)
target_link_libraries(${EX2_NAME}
    PRIVATE
    LumexXml
)

set(EX3_NAME LumexXmlExample3)
add_executable(${EX3_NAME} xml/example_3.cpp)
target_link_libraries(${EX3_NAME}
    PRIVATE
    LumexXml
)

set(EX3_XML_FILE file_for_example_3.xml)
add_custom_command(
    TARGET ${EX3_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/xml/${EX3_XML_FILE}"
    "$<TARGET_FILE_DIR:${EX3_NAME}>/${EX3_XML_FILE}"
    COMMENT "Copying ${EX3_XML_FILE} to build directory"
)

set(EX4_NAME LumexHardwareExampleCPUVectorizationCapabilities)
add_executable(${EX4_NAME} hardware/example_cpu_vectorization_capabilities.cpp)
target_link_libraries(${EX4_NAME}
    PRIVATE
    Lumex::hardware
)

set(EX5_NAME LumexHardwareExampleHardwareCapabilities)
add_executable(${EX5_NAME} hardware/example_hardware_capabilities.cpp)
target_link_libraries(${EX5_NAME}
    PRIVATE
    Lumex::hardware
)

install(TARGETS
    ${EX1_NAME}
    ${EX2_NAME}
    ${EX3_NAME}
    ${EX4_NAME}
    ${EX5_NAME}
    DESTINATION bin
)
install(FILES xml/${EX3_XML_FILE} DESTINATION bin)
