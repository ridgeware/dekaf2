if(NOT CMAKE_VERSION VERSION_LESS "3.13.0")
	cmake_policy(SET CMP0076 OLD)
endif()

target_sources(frozen INTERFACE
  $<BUILD_INTERFACE:
    ${CMAKE_CURRENT_LIST_DIR}/algorithm.h
    ${CMAKE_CURRENT_LIST_DIR}/map.h
    ${CMAKE_CURRENT_LIST_DIR}/random.h
    ${CMAKE_CURRENT_LIST_DIR}/set.h
    ${CMAKE_CURRENT_LIST_DIR}/string.h
    ${CMAKE_CURRENT_LIST_DIR}/unordered_map.h
    ${CMAKE_CURRENT_LIST_DIR}/unordered_set.h
    ${CMAKE_CURRENT_LIST_DIR}/bits/algorithms.h
    ${CMAKE_CURRENT_LIST_DIR}/bits/basic_types.h
    ${CMAKE_CURRENT_LIST_DIR}/bits/elsa.h
    ${CMAKE_CURRENT_LIST_DIR}/bits/pmh.h>
  $<INSTALL_INTERFACE:
    ${dekaf2_include_dest}/frozen/algorithm.h
    ${dekaf2_include_dest}/frozen/map.h
    ${dekaf2_include_dest}/frozen/random.h
    ${dekaf2_include_dest}/frozen/set.h
    ${dekaf2_include_dest}/frozen/string.h
    ${dekaf2_include_dest}/frozen/unordered_map.h
    ${dekaf2_include_dest}/frozen/unordered_set.h
    ${dekaf2_include_dest}/frozen/bits/algorithms.h
    ${dekaf2_include_dest}/frozen/bits/basic_types.h
    ${dekaf2_include_dest}/frozen/bits/elsa.h
    ${dekaf2_include_dest}/frozen/bits/pmh.h>)
