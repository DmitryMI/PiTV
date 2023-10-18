set(MONGOOSE_ROOT "C:/FilesAndFolders/mongoose" CACHE STRING "Path to Mongoose Server source code" FORCE)
set(BOOST_ROOT "C:/FilesAndFolders/boost_1_83_0" CACHE STRING "Path to boost source code" FORCE)
set(SPDLOG_BUILD_ROOT "C:/FilesAndFolders/spdlog/build" CACHE STRING "Path to spdlog build" FORCE)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME current_source_dir_name)
message(STATUS ${current_source_dir_name})

project(${current_source_dir_name})

add_executable (${PROJECT_NAME} "PiTvMain.cpp" "PiTvMain.h" "${MONGOOSE_ROOT}/mongoose.h" "${MONGOOSE_ROOT}/mongoose.c" "PiTvServer.h" "PiTvServer.cpp" "video/Pipeline.h" "video/Pipeline.cpp" "accounts/UserDb.h" "accounts/UserDbCsv.h" "accounts/UserDbCsv.cpp" "accounts/UserDb.cpp" "SystemStats.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
endif()

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
	PkgConfig::gstreamer
	PkgConfig::gstreamer-sdp
	PkgConfig::gstreamer-app
	PkgConfig::gstreamer-video
)


# Mongoose
if(DEFINED CACHE{MONGOOSE_ROOT})
	message(STATUS "Using MONGOOSE_ROOT from cmake cache: ${MONGOOSE_ROOT}")
	target_include_directories(${PROJECT_NAME} PUBLIC "${MONGOOSE_ROOT}")
elseif(DEFINED ENV{MONGOOSE_ROOT})
	message(STATUS "Using MONGOOSE_ROOT from environment variable: $ENV{MONGOOSE_ROOT}")
	target_include_directories(${PROJECT_NAME} PUBLIC "$ENV{MONGOOSE_ROOT}")
else()
	message(FATAL_ERROR "MONGOOSE_ROOT not set!" )
endif()


# Boost
if(DEFINED CACHE{BOOST_ROOT})
	message(STATUS "Using BOOST_ROOT from cmake cache: ${BOOST_ROOT}")
	set(BOOST_ROOT_INTERNAL ${BOOST_ROOT})
elseif(DEFINED ENV{BOOST_ROOT})
	message(STATUS "Using BOOST_ROOT from environment variable: $ENV{BOOST_ROOT}")
	set(BOOST_ROOT_INTERNAL $ENV{BOOST_ROOT})
else()
	message(FATAL_ERROR "BOOST_ROOT not set!" )
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${BOOST_ROOT_INTERNAL})
target_link_directories(${PROJECT_NAME} PUBLIC "${BOOST_ROOT_INTERNAL}/stage/lib")


# Spdlog
if(DEFINED CACHE{SPDLOG_ROOT})
	message(STATUS "Using SPDLOG_ROOT from cmake cache: ${SPDLOG_BUILD_ROOT}")
	list(APPEND CMAKE_PREFIX_PATH ${SPDLOG_BUILD_ROOT})
else()
	message(FATAL_ERROR "SPDLOG_ROOT not set!")
endif()

find_package(spdlog REQUIRED)
target_link_libraries(${PROJECT_NAME} spdlog::spdlog)


if(WIN32)
	add_compile_definitions(CM_WIN32)
	target_link_libraries(${PROJECT_NAME} pdh.lib)
elseif(UNIX)
	add_compile_definitions(CM_UNIX)
else()
	add_compile_definitions(CM_OS_UNKNOWN)
endif()
