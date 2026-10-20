find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)

add_executable(core-tests core_tests.cpp)
target_link_libraries(core-tests PRIVATE qchat_core)
qchat_options(core-tests)
add_test(NAME core COMMAND core-tests)

add_executable(network-tests network_tests.cpp)
target_link_libraries(network-tests PRIVATE qchat_network)
qchat_options(network-tests)
add_test(NAME network COMMAND network-tests)
set_tests_properties(core network PROPERTIES TIMEOUT 30)

if(QCHAT_BUILD_SERVER)
    add_test(NAME server-process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/server_process_test.py" "$<TARGET_FILE:qchat-server>")
    set_tests_properties(server-process PROPERTIES TIMEOUT 30)
endif()

if(QCHAT_BUILD_CLIENT)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(desktop-tests desktop_tests.cpp)
    target_link_libraries(desktop-tests PRIVATE qchat_desktop Qt6::Test)
    qchat_options(desktop-tests)
    add_test(NAME desktop COMMAND desktop-tests)
    add_test(NAME qml-startup COMMAND qchat-client --smoke-test)
    set_tests_properties(desktop qml-startup PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QPA_PLATFORMTHEME="
        TIMEOUT 30)
endif()
add_test(NAME build-tools COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/build_tools_tests.py")
set_tests_properties(build-tools PROPERTIES TIMEOUT 30)
