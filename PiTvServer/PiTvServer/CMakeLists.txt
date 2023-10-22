set(CONFIG_MONGOOSE_ROOT "C:/FilesAndFolders/mongoose" CACHE STRING "Path to Mongoose Server source code")
set(CONFIG BOOST_ROOT "C:/FilesAndFolders/boost_1_83_0" CACHE STRING "Path to boost source code")
set(CONFIG_SPDLOG_BUILD_ROOT "C:/FilesAndFolders/spdlog/build" CACHE STRING "Path to spdlog build")

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME current_source_dir_name)
message(STATUS ${current_source_dir_name})

project(${current_source_dir_name})

add_executable (${PROJECT_NAME} "PiTvMain.cpp" "PiTvMain.h" "PiTvServer.h" "PiTvServer.cpp" "video/Pipeline.h" "video/Pipeline.cpp" "accounts/UserDb.h" "accounts/UserDbCsv.h" "accounts/UserDbCsv.cpp" "accounts/UserDb.cpp" "SystemStats.h")

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Project name deduced to be ${PROJECT_NAME}")

if (MSVC)
	set(ENV{PKG_CONFIG_PATH} "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib/pkgconfig")
endif()


# Gstreamer
find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	PkgConfig::gstreamer
	PkgConfig::gstreamer-sdp
	PkgConfig::gstreamer-app
	PkgConfig::gstreamer-video
)


# Mongoose

if(DEFINED CACHE{CONFIG_MONGOOSE_ROOT})
	message(STATUS "Using MONGOOSE_ROOT from cmake cache: ${CONFIG_MONGOOSE_ROOT}")
	target_include_directories(${PROJECT_NAME} PUBLIC "${CONFIG_MONGOOSE_ROOT}")
	set(MONGOOSE_ROOT ${CONFIG_MONGOOSE_ROOT})
elseif(DEFINED ENV{MONGOOSE_ROOT})
	message(STATUS "Using MONGOOSE_ROOT from environment variable: $ENV{MONGOOSE_ROOT}")
	target_include_directories(${PROJECT_NAME} PUBLIC "$ENV{CONFIG_MONGOOSE_ROOT}")
	set(MONGOOSE_ROOT $ENV{CONFIG_MONGOOSE_ROOT})
else()
	message(STATUS "MONGOOSE_ROOT not set!" )
endif()

target_sources (${PROJECT_NAME} PRIVATE "${MONGOOSE_ROOT}/mongoose.h" "${MONGOOSE_ROOT}/mongoose.c")


# Boost
if(DEFINED CACHE{CONFIG_BOOST_ROOT})
	message(STATUS "Using BOOST_ROOT from cmake cache: ${CONFIG_BOOST_ROOT}")
elseif(DEFINED ENV{BOOST_ROOT})
	message(STATUS "Using BOOST_ROOT from environment variable: $ENV{CONFIG BOOST_ROOT}")
	set(BOOST_ROOT $ENV{BOOST_ROOT})
else()
	message(STATUS "BOOST_ROOT not set!" )
endif()

FIND_PACKAGE( Boost 1.83 COMPONENTS program_options REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

TARGET_LINK_LIBRARIES( ${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES} )


# Spdlog
if(DEFINED CACHE{CONFIG_SPDLOG_BUILD_ROOT})
	message(STATUS "Using SPDLOG_ROOT from cmake cache: ${CONFIG_SPDLOG_BUILD_ROOT}")
	list(APPEND CMAKE_PREFIX_PATH ${CONFIG_SPDLOG_BUILD_ROOT})
else()
	message(STATUS "SPDLOG_ROOT not set!")
endif()

find_package(spdlog REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)


if(WIN32)
	add_compile_definitions(CM_WIN32)
	target_link_libraries(${PROJECT_NAME} pdh.lib)
elseif(UNIX)
	add_compile_definitions(CM_UNIX)
else()
	add_compile_definitions(CM_OS_UNKNOWN)
endif()
