# AutoVizClient third_party

此目录保存 Client 本机使用的第三方 SDK，不提交二进制文件。

约定布局：

```text
third_party/
  AutoVizProto/
    include/
    lib/
  protobuf/       # 可选：protobuf 的本地安装前缀
    include/
    lib/
  Qt5/            # 可选：Qt kit 根目录或指向它的目录链接
    include/
    lib/
```

Client CMake 会自动把以上目录加入搜索路径，不需要设置
`AutoVizProto_DIR`、`Protobuf_DIR`、`Qt5_DIR` 或环境变量。

AutoVizProto 不能复制 Linux 产物到 Windows；必须使用与 Client 相同的 Windows
编译器和架构构建，或解压对应平台的预编译 SDK。

Linux 使用仓库根目录的 `./AutoVizProto/scripts/bootstrap_proto.sh` 生成此 SDK；
Windows 使用 `.\AutoVizProto\scripts\bootstrap_proto.ps1`。两个脚本都只在
`AutoVizProto/build` 构建 Proto。Client 本身只在 `AutoVizClient/build` 构建，
不在 feature 根目录创建 `build/`。
