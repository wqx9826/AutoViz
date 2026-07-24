from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory("autoviz_server"),
        "config",
        "robot_ws.yaml",
    )
    return LaunchDescription(
        [
            Node(
                package="autoviz_server",
                executable="autoviz_server_node",
                name="autoviz_server",
                output="screen",
                parameters=[config],
            )
        ]
    )
