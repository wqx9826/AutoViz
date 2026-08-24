# AutoVizClient third_party

```text
third_party/
  protobuf/      # 可选本机 protobuf SDK，不提交
  Qt6/Qt5/       # 可选本机 Qt Kit 或目录链接，不提交
```

Protocol 源码位于仓库根目录的 `AutoVizProto/`。Client CMake 直接 `add_subdirectory()` 编译
其中的 `.proto` 和 FrameCodec，不接受预编译 AutoVizProto SDK，也不使用 `AutoVizProto_DIR`。

官方 protobuf/Qt 仍必须与 Client 编译器和架构 ABI 匹配；本机依赖根目录使用
`AUTOVIZ_THIRD_PARTY_DIR`。
