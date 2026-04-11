# AutoViz Message Headers

该目录用于存放用户自己的 ROS 消息头文件，供 `src/core/ros/parsers/` 中的解析模板引用。

目录约定：

- `ros1/`
  - 放置 ROS1 环境下已经生成好的消息头文件。
- `ros2/`
  - 放置 ROS2 环境下已经生成好的消息头文件。
- `custom/`
  - 放置项目内部封装、二次整理或临时桥接用的头文件。

使用说明：

1. 将你已经生成好的消息头文件按 ROS 版本放到对应目录。
2. 在 parser 模板中 `#include` 对应头文件。
3. 在 parser 的 `parse...Message(...)` 函数里，把 ROS msg 字段映射到 AutoViz 内部标准模型。
4. 如果你有同一业务对象的 ROS1 / ROS2 两套消息定义，建议分别放在 `ros1/` 和 `ros2/` 中维护。

注意：

- 这里当前不强制提供任何消息头文件。
- AutoViz 主视图不会直接依赖这些头文件，只会依赖内部标准模型。
- 真实 topic 订阅、callback 绑定和字段映射逻辑，请在 `src/core/ros/` 与 `src/core/ros/parsers/` 下补充。
