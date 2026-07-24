# AutoVizClient

AutoVizClient 是独立的 Qt 可视化客户端。整个目录可以单独复制、构建和部署，不依赖
AutoVizServer、ROS2、custom_msgs 或仓库父目录。

## 依赖

- CMake 3.16+
- C++17 编译器
- Qt5 Widgets、Network
- protobuf 3
- GTest（仅在 `BUILD_TESTING=ON` 时需要）

## Linux 构建

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/AutoViz
```

`proto/` 中的 schema 由本工程 CMake 直接编译，生成文件只写入构建目录。`configs/`
会在构建后复制到可执行文件旁。

## Windows 构建

在已安装 Qt5、protobuf 和可选 GTest 的 Developer Command Prompt 中执行：

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\AutoViz.exe
```

实际 Qt 路径和生成器按本机安装调整。客户端默认连接 `127.0.0.1:39090`，也可以在
“连接 -> 连接 Server...”中修改。
