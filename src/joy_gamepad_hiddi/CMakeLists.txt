cmake_minimum_required(VERSION 3.5)
project(joy_gamepad_hiddi)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(joy_gamepad_node
  src/joy_node.cpp
  src/main.cpp
  src/generic_gamepad.cpp
)

target_include_directories(joy_gamepad_node PUBLIC include)
target_link_libraries(joy_gamepad_node hiddi spdlog json)

ament_target_dependencies(joy_gamepad_node
  "rclcpp"
  "sensor_msgs"
)

install(TARGETS
  joy_gamepad_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
