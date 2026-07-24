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
    "${AUTOVIZ_THIRD_PARTY_DIR}/Qt5"
)

find_package(AutoVizProto CONFIG REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Network Widgets)

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
        AutoVizProto::AutoVizProto
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
