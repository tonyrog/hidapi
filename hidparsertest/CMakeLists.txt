message( "===hidparsertest cmakelists===" )

include_directories(
  ${CMAKE_BINARY_DIR}
  ${hidapi_SOURCE_DIR}/hidapi/
  ${hidapi_SOURCE_DIR}/hidapi_parser/
)

add_executable( hidparsertest hidparsertest.c )

target_link_libraries(hidparsertest hidapi hidapi_parser )

install(TARGETS hidparsertest DESTINATION bin)