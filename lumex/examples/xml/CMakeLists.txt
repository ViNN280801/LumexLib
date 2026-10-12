# Standalone after install: configure this directory with LumexLib_DIR set.
# In-tree: gated by LUMEX_BUILD_XML.

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/LumexExampleHelpers.cmake)
lumex_example_executable(
  NAME LumexXmlExample1
  SOURCE example_1.cpp
  LINK lumex::xml
  REQUIRE_TARGET lumex::xml
)
lumex_example_executable(
  NAME LumexXmlExample2
  SOURCE example_2.cpp
  LINK lumex::xml
  REQUIRE_TARGET lumex::xml
)
lumex_example_executable(
  NAME LumexXmlExample3
  SOURCE example_3.cpp
  LINK lumex::xml
  REQUIRE_TARGET lumex::xml
)
lumex_example_executable(
  NAME LumexXmlExample4
  SOURCE example_4.cpp
  LINK lumex::xml
  REQUIRE_TARGET lumex::xml
)

if(TARGET LumexXmlExample3)
  set(EX3_XML_FILE file_for_example_3.xml)
  add_custom_command(
    TARGET LumexXmlExample3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
      "${CMAKE_CURRENT_SOURCE_DIR}/${EX3_XML_FILE}"
      "$<TARGET_FILE_DIR:LumexXmlExample3>/${EX3_XML_FILE}"
    COMMENT "Copying ${EX3_XML_FILE} to build directory"
  )
  if(LUMEX_INSTALL)
    install(FILES ${EX3_XML_FILE} DESTINATION bin)
  endif()
endif()
