cmake_minimum_required(VERSION 3.16)
project(CatchChartEditor VERSION 0.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64" ${CMAKE_PREFIX_PATH})

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia LinguistTools)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/model/Note.cpp
    src/model/BpmEntry.cpp
    src/model/MetaData.cpp
    src/model/Chart.cpp
    src/model/Skin.cpp
    src/utils/MathUtils.cpp
    src/utils/FileUtils.cpp
    src/utils/Settings.cpp
    src/utils/Logger.cpp
    src/utils/Translator.cpp
    src/utils/PerformanceTimer.cpp
    src/utils/DiagnosticCollector.cpp
    src/file/ChartIO.cpp
    src/file/SkinIO.cpp
    src/file/ProjectIO.cpp
    src/file/PluginLoader.cpp
    src/audio/AudioPlayer.cpp
    src/audio/NoteSoundPlayer.cpp
    src/controller/ChartController.cpp
    src/controller/SelectionController.cpp
    src/controller/PlaybackController.cpp
    src/render/NoteRenderer.cpp
    src/render/GridRenderer.cpp
    src/render/BackgroundRenderer.cpp
    src/render/HyperfruitDetector.cpp
    src/plugin/PluginManager.cpp
    src/ui/CustomWidgets/RightPanel.cpp
    src/ui/NoteEditPanel.cpp
    src/ui/BPMTimePanel.cpp
    src/ui/MetaEditPanel.cpp
    src/ui/SpeedPopup.cpp
    src/ui/LeftPanel.cpp
    src/ui/TimelineWidget.cpp
    src/ui/DensityCurve.cpp
    src/ui/NoteItem.cpp
    src/ui/CustomWidgets/ChartCanvas.cpp
    src/ui/dialogs/LogSettingsDialog.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
)

# 关键：添加 src 到包含目录，使 #include "model/Note.h" 能正确找到
target_include_directories(${PROJECT_NAME} PRIVATE src)

# 翻译文件（可选）
# qt6_add_translations(${PROJECT_NAME}
#     RESOURCE_PREFIX /translations
#     TS_FILES resources/translations/catch_editor_zh_CN.ts
#              resources/translations/catch_editor_en_US.ts
#              resources/translations/catch_editor_ja_JP.ts
# )

# 复制默认皮肤
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources/default_skin
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/skins/default
)

if(EXISTS "${CMAKE_SOURCE_DIR}/resources/note_sounds")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources/note_sounds
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/note_sounds
    )
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY resources/ DESTINATION share/${PROJECT_NAME})
