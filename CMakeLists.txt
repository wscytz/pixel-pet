cmake_minimum_required(VERSION 3.21)
project(pixel-pet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
    # macOS Homebrew Qt 自动探测
    if(NOT CMAKE_PREFIX_PATH)
        if(EXISTS "/opt/homebrew/opt/qt")
            list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt")
        elseif(EXISTS "/usr/local/opt/qt")
            list(PREPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt")
        endif()
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia WebSockets)

set(PIXELPET_SOURCES
    src/main.cpp
    src/ui/BeadGrid.h
    src/ui/PixelFaces.h        src/ui/PixelFaces.cpp
    src/ui/Palette.h           src/ui/Palette.cpp
    src/ui/PetWidget.h         src/ui/PetWidget.cpp
    src/ui/SettingsDialog.h    src/ui/SettingsDialog.cpp
    src/ui/FocusStats.h        src/ui/FocusStats.cpp
    src/ui/PetScene.h          src/ui/PetScene.cpp
    src/ui/PetAnimator.h       src/ui/PetAnimator.cpp
    src/dsp/FFT.h              src/dsp/FFT.cpp
    src/dsp/FeatureExtractor.h src/dsp/FeatureExtractor.cpp
    src/dsp/SectionDetector.h  src/dsp/SectionDetector.cpp
    src/dsp/RhythmDetector.h   src/dsp/RhythmDetector.cpp
    src/emotion/Emotion.h
    src/emotion/EmotionMapper.h src/emotion/EmotionMapper.cpp
    src/audio/AudioEngine.h     src/audio/AudioEngine.cpp
    src/audio/WebAudioSource.h  src/audio/WebAudioSource.cpp
    src/audio/SystemAudioSource.h
)

if(APPLE)
    enable_language(OBJCXX)
    list(APPEND PIXELPET_SOURCES src/ui/MacPlatform.mm)
elseif(WIN32)
    list(APPEND PIXELPET_SOURCES src/audio/SystemAudioSource_win.cpp)
endif()

qt6_add_executable(pixel-pet ${PIXELPET_SOURCES})

# 平台相关:macOS 出 .app bundle,Windows 出 GUI 子系统 exe(无控制台窗口)
if(APPLE)
    set_target_properties(pixel-pet PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PixelPet"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.wscytz.pixelpet"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0")
elseif(WIN32)
    set_target_properties(pixel-pet PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(pixel-pet PRIVATE src)
target_link_libraries(pixel-pet PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::WebSockets)

if(APPLE)
    target_link_libraries(pixel-pet PRIVATE "-framework AppKit")  # NSWindow setLevel(MacPlatform.mm)
    # 应用图标:assets/icon.icns → bundle Resources + Info.plist CFBundleIconFile
    set(_icon ${CMAKE_SOURCE_DIR}/assets/icon.icns)
    if(EXISTS "${_icon}")
        target_sources(pixel-pet PRIVATE ${_icon})
        set_source_files_properties(${_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        set_target_properties(pixel-pet PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)
    endif()
endif()

if(WIN32)
    target_link_libraries(pixel-pet PRIVATE ole32 mmdevapi)  # WASAPI loopback(SystemAudioSource_win.cpp;COM GUID 已在 cpp 手工定义)
endif()

# 调参自检工具:无 Qt 头less 音频特征分析(不参与发布,offline 跑真歌迭代算法)
add_executable(analyze tools/analyze.cpp
    src/dsp/FFT.h              src/dsp/FFT.cpp
    src/dsp/FeatureExtractor.h src/dsp/FeatureExtractor.cpp
    src/dsp/SectionDetector.h  src/dsp/SectionDetector.cpp
    src/dsp/RhythmDetector.h   src/dsp/RhythmDetector.cpp
    src/emotion/Emotion.h
    src/emotion/EmotionMapper.h src/emotion/EmotionMapper.cpp)
target_include_directories(analyze PRIVATE src)
if(NOT MSVC)
    target_compile_options(analyze PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 单元测试:纯算法(FFT/EmotionMapper),无 Qt 无框架,CI Regression 跑。
add_executable(pp_test tools/test.cpp
    src/dsp/FFT.h              src/dsp/FFT.cpp
    src/emotion/Emotion.h
    src/emotion/EmotionMapper.h src/emotion/EmotionMapper.cpp)
target_include_directories(pp_test PRIVATE src)
if(NOT MSVC)
    target_compile_options(pp_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 纯算法工具不涉 Q_OBJECT,关 AUTOMOC 免扫源码 + 生成空 mocs_compilation
foreach(t analyze pp_test)
    set_target_properties(${t} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
endforeach()

# 警告标志仅 GNU/Clang(MSVC 不认 -W 系列)
# -Werror=switch:漏枚举 case 一律构建失败(扩 Tier 时兜住 Palette/canonical 等穷举 switch)
if(NOT MSVC)
    target_compile_options(pixel-pet PRIVATE -Wall -Wextra -Wpedantic -Werror=switch)
else()
    target_compile_options(pixel-pet PRIVATE /W4)   # MSVC:显式警告等级(CI 过不代表无警告)
endif()
