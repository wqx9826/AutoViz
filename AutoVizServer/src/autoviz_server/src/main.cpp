#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "autoviz_server/AutoVizServerNode.h"

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<autoviz_server::AutoVizServerNode>());
    rclcpp::shutdown();
    return 0;
}
