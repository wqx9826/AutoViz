cmake_minimum_required(VERSION 3.16)

project(AutoVizClient VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(AUTOVIZ_BUILD_TESTS "Build AutoVizClient GTest executable" OFF)

find_package(Protobuf REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Network Widgets)

set(AUTOVIZ_PROTOCOL_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(AUTOVIZ_PROTOCOL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AUTOVIZ_PROTOCOL_FILES
    autoviz/common.proto
    autoviz/vehicle.proto
    autoviz/planning.proto
    autoviz/perception.proto
    autoviz/control.proto
    autoviz/runtime.proto
    autoviz/transport.proto
)

set(AUTOVIZ_PROTOCOL_GENERATED_SOURCES)
set(AUTOVIZ_PROTOCOL_GENERATED_HEADERS)
set(AUTOVIZ_PROTOCOL_ABSOLUTE_FILES)
foreach(proto_file IN LISTS AUTOVIZ_PROTOCOL_FILES)
    string(REPLACE ".proto" ".pb.cc" generated_source "${proto_file}")
    string(REPLACE ".proto" ".pb.h" generated_header "${proto_file}")
    list(APPEND AUTOVIZ_PROTOCOL_GENERATED_SOURCES
         "${AUTOVIZ_PROTOCOL_GENERATED_DIR}/${generated_source}")
    list(APPEND AUTOVIZ_PROTOCOL_GENERATED_HEADERS
         "${AUTOVIZ_PROTOCOL_GENERATED_DIR}/${generated_header}")
    list(APPEND AUTOVIZ_PROTOCOL_ABSOLUTE_FILES
         "${AUTOVIZ_PROTOCOL_PROTO_ROOT}/${proto_file}")
endforeach()

add_custom_command(
    OUTPUT ${AUTOVIZ_PROTOCOL_GENERATED_SOURCES} ${AUTOVIZ_PROTOCOL_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOVIZ_PROTOCOL_GENERATED_DIR}"
    COMMAND protobuf::protoc
            --cpp_out=${AUTOVIZ_PROTOCOL_GENERATED_DIR}
            --proto_path=${AUTOVIZ_PROTOCOL_PROTO_ROOT}
            ${AUTOVIZ_PROTOCOL_ABSOLUTE_FILES}
    DEPENDS ${AUTOVIZ_PROTOCOL_ABSOLUTE_FILES}
    COMMENT "Generating AutoVizClient protobuf sources"
    VERBATIM
)

add_library(autoviz_client_protocol STATIC
    ${AUTOVIZ_PROTOCOL_GENERATED_SOURCES}
    ${AUTOVIZ_PROTOCOL_GENERATED_HEADERS}
    src/protocol/FrameCodec.h
    src/protocol/FrameCodec.cpp
)
add_library(AutoVizClient::Protocol ALIAS autoviz_client_protocol)

target_include_directories(autoviz_client_protocol
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/protocol"
        "${AUTOVIZ_PROTOCOL_GENERATED_DIR}"
)
target_link_libraries(autoviz_client_protocol PUBLIC protobuf::libprotobuf)
target_compile_features(autoviz_client_protocol PUBLIC cxx_std_17)

set(AUTOVIZ_CLIENT_SOURCES
    src/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/core/model/CommonTypes.h
    src/core/model/VehicleState.h
    src/core/model/VehicleState.cpp
    src/core/model/PathTypes.h
    src/core/model/PathTypes.cpp
    src/core/model/ObstacleTypes.h
    src/core/model/ObstacleTypes.cpp
    src/core/model/ControlTypes.h
    src/core/model/ControlTypes.cpp
    src/core/model/RuntimeStatusTypes.h
    src/core/datacenter/DataManager.h
    src/core/datacenter/DataManager.cpp
    src/core/network/ProtocolModelConverter.h
    src/core/network/ProtocolModelConverter.cpp
    src/core/network/RemoteVisualizationSource.h
    src/core/network/RemoteVisualizationSource.cpp
    src/core/config/VehicleConfig.h
    src/core/config/VehicleConfig.cpp
    src/core/render/SceneManager.h
    src/core/render/SceneManager.cpp
    src/ui/VisualizationView.h
    src/ui/VisualizationView.cpp
    src/ui/ToggleSwitch.h
    src/ui/ToggleSwitch.cpp
    src/ui/MainViewDisplayConfigDialog.h
    src/ui/MainViewDisplayConfigDialog.cpp
    src/ui/LogPanel.h
    src/ui/LogPanel.cpp
    src/ui/status/BottomStatusPanel.h
    src/ui/status/BottomStatusPanel.cpp
    src/ui/network/ConnectionDialog.h
    src/ui/network/ConnectionDialog.cpp
    src/ui/theme/UiScaleManager.h
    src/ui/theme/UiScaleManager.cpp
    src/ui/theme/UiThemeManager.h
    src/ui/theme/UiThemeManager.cpp
    src/ui/charts/ControlDebugData.h
    src/ui/charts/ControlPanelWidget.h
    src/ui/charts/ControlPanelWidget.cpp
    src/ui/charts/ControlPanelStyle.h
    src/ui/charts/ControlPanelStyle.cpp
    src/ui/charts/ControlPlotBuffer.h
    src/ui/charts/ControlPlotBuffer.cpp
    src/ui/charts/PlotCardWidget.h
    src/ui/charts/PlotCardWidget.cpp
    src/ui/charts/StatusSummaryWidget.h
    src/ui/charts/StatusSummaryWidget.cpp
    src/utils/Logger.h
    src/utils/Logger.cpp
)

add_executable(AutoViz ${AUTOVIZ_CLIENT_SOURCES})

target_include_directories(AutoViz PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AutoViz
    PRIVATE
        AutoVizClient::Protocol
        Qt5::Network
        Qt5::Widgets
)

add_custom_command(
    TARGET AutoViz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/configs"
            "$<TARGET_FILE_DIR:AutoViz>/configs"
    COMMENT "Copying AutoVizClient runtime configuration"
)

if(AUTOVIZ_BUILD_TESTS)
    find_package(GTest REQUIRED)

    add_executable(autoviz_client_protocol_tests tests/FrameCodecTest.cpp)
    target_link_libraries(autoviz_client_protocol_tests
        PRIVATE
            AutoVizClient::Protocol
            GTest::gtest_main
    )
endif()
