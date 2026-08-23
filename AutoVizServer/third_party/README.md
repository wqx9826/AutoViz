# AutoVizServer third_party

```text
third_party/
  COLCON_IGNORE  # 防止 colcon 把第三方源码识别成 ROS workspace 包
  AutoVizProto/  # 私有 AutoVizProto 仓库的源码 submodule，必须提交 gitlink
  protobuf/      # 仅在不使用系统 protobuf 时需要，不提交
```

Server 的 ament CMake 直接 `add_subdirectory()` 编译 AutoVizProto，不使用预编译 SDK 或
`AutoVizProto_DIR`。ROS2 和 robot_ws 仍按正常方式 source，Server 仍只从
`AutoVizServer` 目录执行 `colcon build`。

非标准 Protocol 源码位置可通过 `AUTOVIZ_PROTO_SOURCE_DIR` 显式覆盖；本机官方依赖根目录
使用 `AUTOVIZ_THIRD_PARTY_DIR`。
