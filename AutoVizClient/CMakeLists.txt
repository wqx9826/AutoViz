cmake_minimum_required(VERSION 3.16)

project(AutoVizClient VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
# CLion bundled MinGW 的 AutoMoc 子进程不会继承其 bin 路径，因而无法运行
# cc1plus 来生成 moc_predefs.h。项目不依赖该预定义宏文件，关闭探测即可正常生成 MOC。
set(CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF)

set(AUTOVIZ_THIRD_PARTY_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    CACHE PATH "AutoVizClient third-party SDK directory"
)

if(WIN32)
    # Windows 的 Kit 由 IDE 或 CMake cache 显式选择。不要在此处猜测开发机路径，
    # 更不要自动混入 MSYS2 的运行库目录。
    set(AUTOVIZ_QT_ROOT
        "C:/msys64/ucrt64"
        CACHE PATH "Qt 6 MinGW kit root (contains bin/ and lib/)"
    )
    set(AUTOVIZ_QT_PLUGINS_DIR
        "${AUTOVIZ_QT_ROOT}/share/qt6/plugins"
        CACHE PATH "Qt 6 plugin directory (contains platforms/ and sqldrivers/)"
    )
    if(EXISTS "${AUTOVIZ_QT_ROOT}/lib/cmake/Qt6")
        list(PREPEND CMAKE_PREFIX_PATH "${AUTOVIZ_QT_ROOT}")
    endif()
    set(AUTOVIZ_QT_PACKAGE Qt6)
else()
    # Linux 当前统一使用 Qt 5。
    set(AUTOVIZ_QT_PACKAGE Qt5)
endif()

list(PREPEND CMAKE_PREFIX_PATH
    "${AUTOVIZ_THIRD_PARTY_DIR}/AutoVizProto"
    "${AUTOVIZ_THIRD_PARTY_DIR}/protobuf"
    "${AUTOVIZ_THIRD_PARTY_DIR}/Qt6"
    "${AUTOVIZ_THIRD_PARTY_DIR}/Qt5"
)

find_package(AutoVizProto CONFIG REQUIRED)
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Network Sql Widgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS Network Sql Widgets)
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
target_link_libraries(AutoViz
    PRIVATE
        AutoVizProto::AutoVizProto
        ${AUTOVIZ_QT_PACKAGE}::Network
        ${AUTOVIZ_QT_PACKAGE}::Sql
        ${AUTOVIZ_QT_PACKAGE}::Widgets
)

add_custom_command(
    TARGET AutoViz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/configs"
            "$<TARGET_FILE_DIR:AutoViz>/configs"
    COMMENT "Copying AutoVizClient runtime configuration"
)

include(GNUInstallDirs)
install(TARGETS AutoViz
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY configs/
    DESTINATION "${CMAKE_INSTALL_BINDIR}/configs"
)

# MSYS2 Qt 的编译期插件目录位于 share/qt6/plugins。发布包必须用 qt.conf
# 将插件搜索根重定向到应用目录中的 plugins/，否则双击启动无法发现 qwindows.dll。
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/deploy/qt.conf"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(WIN32)
    # 只从当前选定 Kit 的 bin 解析运行依赖，禁止把 Qt Online Installer、
    # CLion 或其他 MinGW 目录混入发布包。
    install(CODE [=[
        if(POLICY CMP0207)
            cmake_policy(SET CMP0207 NEW)
        endif()
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${CMAKE_INSTALL_PREFIX}/bin/AutoViz.exe"
            RESOLVED_DEPENDENCIES_VAR autoviz_runtime_dependencies
            UNRESOLVED_DEPENDENCIES_VAR autoviz_unresolved_dependencies
            DIRECTORIES
                "C:/msys64/ucrt64/bin"
            PRE_EXCLUDE_REGEXES
                "^api-ms-win-"
                "^ext-ms-win-"
            POST_EXCLUDE_REGEXES
                "^[A-Za-z]:/Windows/"
                "^api-ms-win-"
                "^ext-ms-win-"
                ".*/(advapi32|authz|d3d11|dbghelp|dnsapi|dwmapi|dwrite|dxgi|gdi32|iphlpapi|kernel32|mpr|msvcrt|netapi32|ntdll|ole32|secur32|shell32|user32|userenv|uxtheme|version|winhttp|winmm|ws2_32)\\.dll$"
        )
        if (autoviz_unresolved_dependencies)
            message(FATAL_ERROR "AutoViz package has unresolved DLL dependencies: ${autoviz_unresolved_dependencies}")
        endif()
        file(INSTALL
            DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
            TYPE SHARED_LIBRARY
            FILES ${autoviz_runtime_dependencies}
        )
    ]=])
    install(FILES
        "${AUTOVIZ_QT_PLUGINS_DIR}/platforms/qwindows.dll"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/platforms"
    )
    install(FILES
        "${AUTOVIZ_QT_PLUGINS_DIR}/sqldrivers/qsqlite.dll"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/sqldrivers"
    )
endif()

add_executable(AutoVizClientPlaybackTests
    tests/PlaybackValidationMain.cpp
    src/core/playback/RobotWsCdrDecoder.h
    src/core/playback/RobotWsCdrDecoder.cpp
)
target_include_directories(AutoVizClientPlaybackTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AutoVizClientPlaybackTests
    PRIVATE AutoVizProto::AutoVizProto ${AUTOVIZ_QT_PACKAGE}::Core ${AUTOVIZ_QT_PACKAGE}::Sql
)

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
target_link_libraries(AutoVizPlaybackSourceSmoke
    PRIVATE AutoVizProto::AutoVizProto ${AUTOVIZ_QT_PACKAGE}::Core ${AUTOVIZ_QT_PACKAGE}::Sql
)
