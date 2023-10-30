cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_qt(COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

set(USE_SYSTEM_CURL
    OFF
    CACHE STRING "Use system cURL")

set(USE_SYSTEM_PUGIXML
    OFF
    CACHE STRING "Use system pugixml")

if(USE_SYSTEM_CURL)
  find_package(CURL REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${CURL_LIBRARIES}")
  target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${CURL_INCLUDE_DIRS}")
else()
  include(cmake/BuildMyCurl.cmake)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libcurl)
endif()

if(USE_SYSTEM_PUGIXML)
  find_package(pugixml REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE pugixml)
else()
  include(cmake/BuildPugiXML.cmake)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libpugixml)
endif()

include(cmake/BuildJSONCONS.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE jsoncons)

include(cmake/BuildInja.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE inja)

include(cmake/BuildLibTidy.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libtidy)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE vendor/nlohmann-json)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c src/url-source.cpp src/ui/RequestBuilder.cpp
                                             src/request-data.cpp src/ui/text-render-helper.cpp src/url-source-info.c)
add_subdirectory(src/parsers)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
