# lumex/xml/CMakeLists.txt

set(LUMEX_XML_NAME LumexXml)
set(EXPORT_TO LumexXmlTargets)

add_library(${LUMEX_XML_NAME} SHARED

    # ======= Core XML files =======
    attribute/XmlAttribute.cpp
    attribute/XmlAttributeBase.cpp
    attribute/XmlAttributeIterator.cpp

    document/XmlDocument.cpp
    document/XmlDocumentBase.cpp

    memory/XmlAllocator.cpp
    memory/XmlMemoryPage.cpp

    node/XmlNamedNodeIterator.cpp
    node/XmlNode.cpp
    node/XmlNodeBase.cpp
    node/XmlNodeIterator.cpp

    text/XmlParser.cpp
    text/XmlParseResult.cpp
    text/XmlText.cpp

    writer/XmlBufferedWriter.cpp
    writer/XmlWriterFile.cpp
    writer/XmlWriterStream.cpp

    # ======= XPath files =======
    xpath/ast/XPathAstNode.cpp

    xpath/document/XPathDocumentOrderComparator.cpp

    xpath/memory/XPathAllocator.cpp
    xpath/memory/XPathStack.cpp

    xpath/node/XPathNode.cpp
    xpath/node/XPathNodeSet.cpp

    xpath/parser/lexer/XPathLexer.cpp
    xpath/parser/lexer/XPathLexerString.cpp

    xpath/parser/XPathParser.cpp
    xpath/parser/XPathParseResult.cpp

    xpath/query/XPathQuery.cpp

    xpath/string/XPathString.cpp

    xpath/utility/XPathUtils.cpp

    xpath/variable/XPathVariable.cpp
    xpath/variable/XPathVariableSet.cpp
)

add_library(Lumex::xml ALIAS ${LUMEX_XML_NAME})

if(BUILD_SHARED_LIBS AND WIN32)
    target_compile_definitions(${LUMEX_XML_NAME} PRIVATE LUMEX_EXPORTS)
endif()

if(MSVC)
    target_compile_options(${LUMEX_XML_NAME} PRIVATE
        /wd4099
        /wd4127
    )
endif()

target_include_directories(${LUMEX_XML_NAME} PUBLIC
    $<INSTALL_INTERFACE:include/lumex/xml>

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/attribute>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/constants>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/document>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/memory>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/node>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/range>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/text>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tree>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/types>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utility>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/writer>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/ast>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/constants>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/context>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/document>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/exception>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/memory>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/node>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/parser>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/parser/lexer>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/query>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/string>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/utility>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xpath/variable>

    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lumex/core/utility>
)

target_link_libraries(${LUMEX_XML_NAME} PUBLIC Lumex::utility)

install(TARGETS ${LUMEX_XML_NAME} EXPORT ${EXPORT_TO}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/attribute
    ${CMAKE_CURRENT_SOURCE_DIR}/constants
    ${CMAKE_CURRENT_SOURCE_DIR}/document
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/node
    ${CMAKE_CURRENT_SOURCE_DIR}/range
    ${CMAKE_CURRENT_SOURCE_DIR}/text
    ${CMAKE_CURRENT_SOURCE_DIR}/tree
    ${CMAKE_CURRENT_SOURCE_DIR}/types
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
    ${CMAKE_CURRENT_SOURCE_DIR}/writer
    ${CMAKE_CURRENT_SOURCE_DIR}/xpath
    DESTINATION include/lumex/xml
    FILES_MATCHING
    PATTERN "*.hpp"
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/LumexXml
    DESTINATION include/lumex/xml
)
