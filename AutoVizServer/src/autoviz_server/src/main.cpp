#include "autoviz_server/AutoVizServerNode.h"

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<autoviz_server::AutoVizServerNode>();
    if (!node->start()) {
        RCLCPP_ERROR(node->get_logger(), "AutoVizServerNode 启动失败");
        rclcpp::shutdown();
        return -1;
    }

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
