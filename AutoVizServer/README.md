# AutoVizServer

AutoVizServer 是独立 ROS2 workspace。当前 `src/autoviz_server` 包订阅 robot_ws
消息，完成单位与坐标归一化，并通过 protobuf/TCP 向只读 Client 发布可视化数据。

## 构建

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash

colcon --log-base log build \
  --base-paths src \
  --build-base build \
  --install-base install
```

## 测试

```bash
colcon --log-base log test \
  --base-paths src \
  --build-base build \
  --install-base install
colcon test-result --test-result-base build --verbose
```

## 运行

```bash
source install/setup.bash
ros2 launch autoviz_server autoviz_server.launch.py
```

`src/autoviz_server/proto/` 是 Server 自己的协议副本，由包内 CMake 直接编译。Server
构建不读取 AutoVizClient 或仓库根目录的源码。
