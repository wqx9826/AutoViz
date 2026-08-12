from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    # 获取autoviz_server包的 share 目录路径
    share_dir = get_package_share_directory("autoviz_server")
    config = os.path.join(
        share_dir,
        "config",
        "robot_ws.yaml",
    )

    autoVizServerNode = Node(
        package="autoviz_server",
        executable="autoviz_server_node",
        name="autoviz_server",
        output="screen",
        parameters=[config],
    )

    return LaunchDescription(
        [
            autoVizServerNode,
        ]
    )
