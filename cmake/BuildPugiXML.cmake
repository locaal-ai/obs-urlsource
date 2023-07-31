include(ExternalProject)

ExternalProject_Add(
  pugixml_build
  GIT_REPOSITORY git@github.com:zeux/pugixml.git
  GIT_TAG v1.13
  GIT_SHALLOW TRUE
  CMAKE_GENERATOR ${CMAKE_GENERATOR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DBUILD_SHARED_LIBS=OFF -DPUGIXML_BUILD_TESTS=OFF)

ExternalProject_Get_Property(pugixml_build INSTALL_DIR)

set(pugixml_DIR
    ${INSTALL_DIR}/lib/cmake/pugixml
    CACHE PATH "Path to internally built pugixmlConfig.cmake")
find_package(pugixml REQUIRED CONFIG PATHS ${pugixml_DIR} NO_DEFAULT_PATH)

add_library(libpugixml INTERFACE)
add_dependencies(libpugixml pugixml_build)
target_link_libraries(libpugixml INTERFACE pugixml::static)
