cmake_minimum_required(VERSION 3.16)

project("PiTVDesktopViewer")

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory("PiTVDesktopViewer")