# AutoVizServer third_party

此目录保存 Server 本机使用的第三方 SDK，不提交二进制文件。

约定布局：

```text
third_party/
  AutoVizProto/
    include/
    lib/
  protobuf/       # 仅在不使用系统 protobuf 时需要
    include/
    lib/
```

`src/autoviz_server/CMakeLists.txt` 会自动搜索这些目录，因此正常构建不需要传
`AutoVizProto_DIR` 或设置额外环境变量。ROS2 和 robot_ws 仍需按正常 ROS2 方式
source。

Linux 下从仓库根目录运行 `./AutoVizProto/scripts/bootstrap_proto.sh` 生成此 SDK。
Proto 产物位于 `AutoVizProto/build`。Server 必须在 `AutoVizServer` 目录中使用
`colcon build`，由 colcon 管理 Server 自己的 `build/`、`install/` 和 `log/`；禁止在
feature 根目录生成构建产物。
