@echo off
REM Windows 构建 + 打包脚本(MSVC + Qt6)
REM 前置:Visual Studio(含 CMake Tools)、Qt6(Widgets + Multimedia)
REM 用法:在项目根目录双击或 cmd 运行此脚本

REM === 按你的 Qt 安装改这一行 ===
if "%QT_PREFIX%"=="" set QT_PREFIX=C:\Qt\6.9.0\msvc2022_64

set CMAKE_PREFIX_PATH=%QT_PREFIX%
if not exist build mkdir build
cd build

echo === Configure ===
cmake .. -DCMAKE_BUILD_TYPE=Release || goto :err

echo === Build ===
cmake --build . --config Release || goto :err

echo === Deploy(打 Qt 依赖) ===
"%QT_PREFIX%\bin\windeployqt.exe" --release --no-translations Release\pixel-pet.exe || goto :err

echo.
echo 完成。可分发:build\Release\pixel-pet.exe(连同同目录的 Qt DLL / platforms 插件)
exit /b 0

:err
echo 构建失败
exit /b 1
