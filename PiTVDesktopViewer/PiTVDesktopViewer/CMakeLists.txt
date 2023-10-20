cmake_minimum_required(VERSION 3.16)
project(PiTVDesktopViewer LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Network
)
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    PiTVDesktopViewer.ui
    PiTVDesktopViewer.h
    PiTVDesktopViewer.cpp
    EditServerDialog.ui
    EditServerDialog.h
    EditServerDialog.cpp
    ServerStatusMessage.h
    resources.qrc
)

set(CMAKE_AUTORCC ON)

qt_add_executable(PiTVDesktopViewer ${PROJECT_SOURCES})

set_target_properties(PiTVDesktopViewer
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_link_libraries(PiTVDesktopViewer
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt6::Widgets
)


# Gstreamer

if (MSVC)
	set(ENV{PKG_CONFIG_PATH} "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib/pkgconfig")
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

target_link_libraries(${PROJECT_NAME}
	PRIVATE PkgConfig::gstreamer
	PRIVATE PkgConfig::gstreamer-sdp
	PRIVATE PkgConfig::gstreamer-app
	PRIVATE PkgConfig::gstreamer-video
)
