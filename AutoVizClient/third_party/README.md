# AutoVizClient third_party

```text
third_party/
  AutoVizProto/  # 私有 AutoVizProto 仓库的源码 submodule，必须提交 gitlink
  protobuf/      # 可选本机 protobuf SDK，不提交
  Qt6/Qt5/       # 可选本机 Qt Kit 或目录链接，不提交
```

`AutoVizProto` 必须用 `git submodule update --init --recursive` 初始化。Client CMake 直接
`add_subdirectory()` 编译其中的 `.proto` 和 FrameCodec，不接受预编译 AutoVizProto SDK，
也不使用 `AutoVizProto_DIR`。

官方 protobuf/Qt 仍必须与 Client 编译器和架构 ABI 匹配。非标准 Protocol 源码位置可通过
`AUTOVIZ_PROTO_SOURCE_DIR` 显式覆盖；本机依赖根目录使用 `AUTOVIZ_THIRD_PARTY_DIR`。
