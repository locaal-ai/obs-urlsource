include(FetchContent)

set(LibCurl_VERSION "8.4.0")
set(LibCurl_BASEURL "https://github.com/obs-ai/obs-ai-libcurl-dep/releases/download/${LibCurl_VERSION}")

if(${CMAKE_BUILD_TYPE} STREQUAL Release OR ${CMAKE_BUILD_TYPE} STREQUAL RelWithDebInfo)
set(LibCurl_BUILD_TYPE Release)
else()
set(LibCurl_BUILD_TYPE Debug)
endif()

if(APPLE)
if(LibCurl_BUILD_TYPE STREQUAL Release)
    set(LibCurl_URL "${LibCurl_BASEURL}/libcurl-macos-Release.tar.gz")
    set(LibCurl_HASH MD5=30FBFA06FCE476B6A8520984703BC2B7)
else()
    set(LibCurl_URL "${LibCurl_BASEURL}/libcurl-macos-Debug.tar.gz")
    set(LibCurl_HASH MD5=E13398CF463F13EDA946712F66883BD0)
endif()
elseif(MSVC)
if(LibCurl_BUILD_TYPE STREQUAL Release)
    set(LibCurl_URL "${LibCurl_BASEURL}/libcurl-windows-Release.zip")
    set(LibCurl_HASH MD5=FC0CC7E9730BDCD9A50312A7D176E963)
else()
    set(LibCurl_URL "${LibCurl_BASEURL}/libcurl-windows-Debug.zip")
    set(LibCurl_HASH MD5=09D73B647EE8CC68408F5276A0DB81EB)
endif()
else()
if(LibCurl_BUILD_TYPE STREQUAL Release)
    set(LibCurl_URL "${LibCurl_BASEURL}/libcurl-linux-Release.tar.gz")
    set(LibCurl_HASH MD5=B058DB62F34DA44688DA829453B4C59D)
else()
    set(LibCurl_URL "${LibCurl_BASEURL}/libcurl-linux-Debug.tar.gz")
    set(LibCurl_HASH MD5=ADF46200BC5DF9E883015B0EFE7F8CCB)
endif()
endif()

FetchContent_Declare(
  libcurl_fetch
  URL ${LibCurl_URL}
  URL_HASH ${LibCurl_HASH})
FetchContent_MakeAvailable(libcurl_fetch)

add_library(libcurl INTERFACE)
if(MSVC)
  target_link_libraries(
    libcurl
    INTERFACE ${libcurl_fetch_SOURCE_DIR}/lib/libcurl.lib)
  target_include_directories(libcurl SYSTEM INTERFACE ${libcurl_fetch_SOURCE_DIR}/include)
else()
  target_link_libraries(
    libcurl INTERFACE ${libcurl_fetch_SOURCE_DIR}/lib/libcurl.a)
  target_include_directories(libcurl SYSTEM INTERFACE ${libcurl_fetch_SOURCE_DIR}/include)
endif()


# if(OS_MACOS)
#   set(CURL_USE_OPENSSL OFF)
#   set(CURL_USE_SECTRANSP ON)
# elseif(OS_WINDOWS)
#   set(CURL_USE_OPENSSL OFF)
#   set(CURL_USE_SCHANNEL ON)
# elseif(OS_LINUX)
#   add_compile_options(-fPIC)
#   set(CURL_USE_OPENSSL ON)
# endif()
# set(BUILD_CURL_EXE OFF)
# set(BUILD_SHARED_LIBS OFF)
# set(HTTP_ONLY ON)
# set(CURL_USE_LIBSSH2 OFF)
# set(BUILD_TESTING OFF)
# set(PICKY_COMPILER OFF)

# add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/curl EXCLUDE_FROM_ALL)
# if(OS_MACOS)
#   target_compile_options(
#     libcurl PRIVATE -Wno-error=ambiguous-macro -Wno-error=deprecated-declarations -Wno-error=unreachable-code
#                     -Wno-error=unused-parameter -Wno-error=unused-variable)
# endif()
# include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/vendor/curl/include)
