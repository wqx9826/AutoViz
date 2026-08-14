cmake_minimum_required(VERSION 3.16)

project(AutoVizClient VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(AUTOVIZ_THIRD_PARTY_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    CACHE PATH "AutoVizClient third-party SDK directory"
)

list(PREPEND CMAKE_PREFIX_PATH
    "${AUTOVIZ_THIRD_PARTY_DIR}/AutoVizProto"
    "${AUTOVIZ_THIRD_PARTY_DIR}/protobuf"
)

find_package(AutoVizProto CONFIG REQUIRED)
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network Sql)
endif()

set(AUTOVIZ_CLIENT_SOURCES
    resources.qrc
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
    src/core/playback/RosbagPlaybackTypes.h
    src/core/playback/RosbagPlaybackTypes.cpp
    src/core/playback/RobotWsCdrDecoder.h
    src/core/playback/RobotWsCdrDecoder.cpp
    src/core/playback/LocalRosbagPlaybackSource.h
    src/core/playback/LocalRosbagPlaybackSource.cpp
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
    src/ui/playback/PlaybackSpeedWidget.h
    src/ui/playback/PlaybackSpeedWidget.cpp
    src/ui/playback/RosbagPlaybackDialog.h
    src/ui/playback/RosbagPlaybackDialog.cpp
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

if(WIN32)
    list(APPEND AUTOVIZ_CLIENT_SOURCES assets/autoviz.rc assets/autoviz_icon.ico)
endif()

if(WIN32)
    # 发布版采用 Windows GUI 子系统：从资源管理器双击时不创建控制台窗口。
    add_executable(AutoViz WIN32 ${AUTOVIZ_CLIENT_SOURCES})
else()
    add_executable(AutoViz ${AUTOVIZ_CLIENT_SOURCES})
endif()

target_include_directories(AutoViz PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AutoViz PRIVATE AutoVizProto::AutoVizProto)
if(WIN32)
    target_link_libraries(AutoViz PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql)
else()
    target_link_libraries(AutoViz PRIVATE
        Qt5::Core Qt5::Widgets Qt5::Network Qt5::Sql)
endif()

add_custom_command(
    TARGET AutoViz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/configs"
            "$<TARGET_FILE_DIR:AutoViz>/configs"
    COMMENT "Copying AutoVizClient runtime configuration"
)

add_executable(AutoVizClientPlaybackTests
    tests/PlaybackValidationMain.cpp
    src/core/playback/RobotWsCdrDecoder.h
    src/core/playback/RobotWsCdrDecoder.cpp
)
target_include_directories(AutoVizClientPlaybackTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AutoVizClientPlaybackTests PRIVATE AutoVizProto::AutoVizProto)
if(WIN32)
    target_link_libraries(AutoVizClientPlaybackTests PRIVATE Qt6::Core Qt6::Sql)
else()
    target_link_libraries(AutoVizClientPlaybackTests PRIVATE Qt5::Core Qt5::Sql)
endif()

add_executable(AutoVizPlaybackSourceSmoke
    tests/PlaybackSourceSmokeMain.cpp
    src/core/model/VehicleState.cpp
    src/core/model/PathTypes.cpp
    src/core/model/ObstacleTypes.cpp
    src/core/model/ControlTypes.cpp
    src/core/datacenter/DataManager.cpp
    src/core/config/VehicleConfig.cpp
    src/core/network/ProtocolModelConverter.cpp
    src/core/playback/RosbagPlaybackTypes.cpp
    src/core/playback/RobotWsCdrDecoder.cpp
    src/core/playback/LocalRosbagPlaybackSource.cpp
)
target_include_directories(AutoVizPlaybackSourceSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AutoVizPlaybackSourceSmoke PRIVATE AutoVizProto::AutoVizProto)
if(WIN32)
    target_link_libraries(AutoVizPlaybackSourceSmoke PRIVATE Qt6::Core Qt6::Sql)
else()
    target_link_libraries(AutoVizPlaybackSourceSmoke PRIVATE Qt5::Core Qt5::Sql)
endif()

# 打包规则独立于正常构建逻辑；仍由标准的 `cmake --install` 执行。
include(cmake/InstallRules.cmake)
