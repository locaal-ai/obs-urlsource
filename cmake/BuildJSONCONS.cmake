set(JSONCONS_BUILD_TESTS OFF)
add_compile_options(-Wno-conversion -Wno-error=conversion)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/jsoncons ${CMAKE_BINARY_DIR}/jsoncons EXCLUDE_FROM_ALL)
