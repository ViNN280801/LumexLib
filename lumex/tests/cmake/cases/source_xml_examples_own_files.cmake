# Every xml example that writes a file gives it a name of its own and removes
# it after use. Examples 1 and 2 once shared ./xgconsole.xml; a parallel ctest
# let one overwrite the file the other was reading, and example 1 failed now
# and then.

file(GLOB _examples "${LUMEX_SOURCE_DIR}/lumex/examples/xml/example_*.cpp")
if(NOT _examples)
    message(FATAL_ERROR "no xml examples found")
endif()

set(_seen "")
foreach(_file IN LISTS _examples)
    file(READ "${_file}" _text)
    # kXmlFilePath = "name.xml";
    if(NOT _text MATCHES "kXmlFilePath = \"([^\"]+)\"")
        message(FATAL_ERROR "${_file} has no kXmlFilePath")
    endif()
    set(_name "${CMAKE_MATCH_1}")
    list(FIND _seen "${_name}" _dup)
    if(NOT _dup EQUAL -1)
        message(FATAL_ERROR
            "two xml examples use the file '${_name}': parallel ctest races")
    endif()
    list(APPEND _seen "${_name}")

    # Examples that save the file also delete it (example 3 only reads a file
    # that ships with the sources).
    if(_text MATCHES "save_file")
        if(NOT _text MATCHES "std::remove \\(kXmlFilePath\\)")
            message(FATAL_ERROR "${_file} writes '${_name}' and leaves it")
        endif()
        if(NOT _name MATCHES "^lumex_xml_example_[0-9]+\\.xml$")
            message(FATAL_ERROR
                "${_file}: the written file '${_name}' is not "
                "lumex_xml_example_<n>.xml")
        endif()
    endif()
endforeach()
