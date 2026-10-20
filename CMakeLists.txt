cmake_minimum_required(VERSION 3.28)
project(QChat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF)
include(CTest)
include(GNUInstallDirs)

option(QCHAT_BUILD_SERVER "Build the Linux server" ${CMAKE_HOST_LINUX})
option(QCHAT_BUILD_CLIENT "Build the Qt Quick desktop client" ON)
option(QCHAT_REQUIRE_STATIC "Reject shared Qt and Thrift dependencies" ON)
option(QCHAT_FULLY_STATIC_SERVER "Link the Linux server with no shared libraries (use musl)" OFF)
option(QCHAT_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
option(QCHAT_CLANG_TIDY "Analyze project sources while compiling" OFF)
option(QCHAT_COVERAGE "Instrument project sources for gcov coverage" OFF)
set(QCHAT_SANITIZER "" CACHE STRING "Sanitizer: address,undefined or thread")

if(QCHAT_FULLY_STATIC_SERVER AND (QCHAT_SANITIZER OR QCHAT_COVERAGE))
    message(FATAL_ERROR "Fully static server and instrumentation require separate builds")
endif()
if(QCHAT_CLANG_TIDY)
    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
endif()
find_package(Threads REQUIRED)
find_package(Thrift 0.25.0 EXACT CONFIG REQUIRED)
find_program(THRIFT_COMPILER NAMES thrift
    HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/thrift"
          "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/thrift"
    REQUIRED)
execute_process(COMMAND "${THRIFT_COMPILER}" -version
    OUTPUT_VARIABLE thrift_compiler_version OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
if(NOT thrift_compiler_version STREQUAL "Thrift version ${THRIFT_VERSION}")
    message(FATAL_ERROR "Thrift compiler/runtime mismatch: ${thrift_compiler_version}, runtime ${THRIFT_VERSION}")
endif()
if(QCHAT_REQUIRE_STATIC)
    get_target_property(thrift_type thrift::thrift TYPE)
    if(NOT thrift_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "Static Thrift is required; use the supplied vcpkg triplets")
    endif()
endif()

function(qchat_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(QCHAT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(QCHAT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
    if(QCHAT_CLANG_TIDY)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
    endif()
    if(QCHAT_SANITIZER)
        target_compile_options(${target} PRIVATE "-fsanitize=${QCHAT_SANITIZER}" -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC "-fsanitize=${QCHAT_SANITIZER}")
    endif()
    if(QCHAT_COVERAGE)
        target_compile_options(${target} PRIVATE --coverage -fprofile-update=atomic -O0 -g)
        target_link_options(${target} PUBLIC --coverage)
    endif()
endfunction()

set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated}")
add_custom_command(
    OUTPUT "${generated}/Chat.cpp" "${generated}/Chat.h"
           "${generated}/chat_types.cpp" "${generated}/chat_types.h"
    COMMAND "${THRIFT_COMPILER}" --gen cpp:no_skeleton,no_default_operators -out "${generated}"
            "${CMAKE_CURRENT_SOURCE_DIR}/protocol/chat.thrift"
    DEPENDS protocol/chat.thrift
    VERBATIM)
add_library(qchat_protocol STATIC "${generated}/Chat.cpp" "${generated}/chat_types.cpp")
target_include_directories(qchat_protocol SYSTEM PUBLIC "${generated}")
target_link_libraries(qchat_protocol PUBLIC thrift::thrift)
# Generated code and dependencies use their upstream warning policies.
if(MSVC)
    target_compile_options(qchat_protocol PRIVATE /utf-8)
endif()

add_library(qchat_core STATIC src/core/store.cpp)
target_include_directories(qchat_core PUBLIC include)
target_link_libraries(qchat_core PUBLIC Threads::Threads)
qchat_options(qchat_core)

add_library(qchat_network STATIC src/network/service.cpp src/network/network.cpp)
target_link_libraries(qchat_network PUBLIC qchat_core qchat_protocol)
qchat_options(qchat_network)

if(QCHAT_BUILD_SERVER)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "The server supports Linux; set QCHAT_BUILD_SERVER=OFF on other systems")
    endif()
    add_executable(qchat-server src/server/main.cpp)
    target_link_libraries(qchat-server PRIVATE qchat_network)
    qchat_options(qchat-server)
    if(QCHAT_FULLY_STATIC_SERVER)
        target_link_options(qchat-server PRIVATE -static)
    endif()
    install(TARGETS qchat-server RUNTIME DESTINATION bin COMPONENT server)
endif()

if(QCHAT_BUILD_CLIENT)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
    if(QCHAT_REQUIRE_STATIC)
        get_target_property(qt_type Qt6::Core TYPE)
        if(NOT qt_type STREQUAL "STATIC_LIBRARY")
            message(FATAL_ERROR "Static Qt is required. For development only, set QCHAT_REQUIRE_STATIC=OFF.")
        endif()
    endif()
    qt_standard_project_setup(REQUIRES 6.8)
    qt_add_library(qchat_desktop STATIC)
    set_source_files_properties(src/client/qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
    qt_add_qml_module(qchat_desktop
        URI QChat VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/QChat"
        QML_FILES src/client/qml/Main.qml
        SOURCES src/client/chat_controller.hpp src/client/chat_controller.cpp
                src/client/message_model.hpp src/client/message_model.cpp)
    target_include_directories(qchat_desktop PUBLIC src/client)
    target_link_libraries(qchat_desktop PUBLIC qchat_network Qt6::Core Qt6::Network Qt6::Qml
        Qt6::Quick Qt6::QuickControls2)
    qchat_options(qchat_desktop)
    qt_add_executable(qchat-client src/client/main.cpp)
    target_link_libraries(qchat-client PRIVATE qchat_desktop qchat_desktopplugin)
    qchat_options(qchat-client)
    if(UNIX AND NOT APPLE AND QCHAT_REQUIRE_STATIC)
        target_link_options(qchat-client PRIVATE -static-libgcc -static-libstdc++)
    endif()
    # Qt imports static platform and QML plugins, including the embedded Basic controls.
    qt_import_qml_plugins(qchat-client)
    if(QCHAT_REQUIRE_STATIC)
        qt_import_plugins(qchat-client INCLUDE Qt6::QOffscreenIntegrationPlugin)
    endif()
    set_target_properties(qchat-client PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.qchat.desktop"
        MACOSX_BUNDLE_BUNDLE_NAME "QChat"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    install(TARGETS qchat-client
        BUNDLE DESTINATION . COMPONENT client
        RUNTIME DESTINATION bin COMPONENT client)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
