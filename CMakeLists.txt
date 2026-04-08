cmake_minimum_required(VERSION 3.16)

project(AutoViz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/ui/TopicConfigPanel.h
    src/ui/TopicConfigPanel.cpp
    src/ui/DetailPanel.h
    src/ui/DetailPanel.cpp
    src/ui/LogPanel.h
    src/ui/LogPanel.cpp
    src/ui/VisualizationView.h
    src/ui/VisualizationView.cpp
    src/core/model/CommonTypes.h
    src/core/model/TrajectoryModel.h
    src/core/model/ObstacleModel.h
    src/core/model/EgoModel.h
    src/core/datasource/ITopicSource.h
    src/core/datasource/IMsgParser.h
    src/core/datasource/TopicConfig.h
    src/core/datasource/MsgDefinition.h
    src/core/datasource/MsgDefinition.cpp
    src/core/datasource/MsgRegistry.h
    src/core/datasource/MsgRegistry.cpp
    src/core/datasource/MsgCompiler.h
    src/core/datasource/MsgCompiler.cpp
    src/core/datasource/RosMsgPackage.h
    src/core/datasource/RosMsgPackage.cpp
    src/core/datasource/RosMsgPackageRegistry.h
    src/core/datasource/RosMsgPackageRegistry.cpp
    src/core/render/SceneManager.h
    src/core/render/SceneManager.cpp
    src/ui/ControlStatusPanel.h
    src/ui/ControlStatusPanel.cpp
    src/utils/Logger.h
    src/utils/Logger.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt5::Widgets
)
