set(OPENSSL_ROOT "C:/FilesAndFolders/mongoose" CACHE STRING "Path to Mongoose Server source code")
set(MONGOOSE_ROOT "C:/FilesAndFolders/OpenSSL-Win64" CACHE STRING "Path to OpenSSL installation")
set(BOOST_ROOT "C:/FilesAndFolders/boost_1_80_0" CACHE STRING "Path to boost source code")
set(SPDLOG_ROOT "C:/FilesAndFolders/spdlog/build" CACHE STRING "Path to spdlog build")

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME current_source_dir_name)
message(STATUS ${current_source_dir_name})

project(${current_source_dir_name})

add_executable (${PROJECT_NAME} "PiTvMain.cpp"  "PiTvServer.h" "PiTvServer.cpp" "video/Pipeline.h" "video/Pipeline.cpp" "accounts/UserDb.h" "accounts/UserDbCsv.h" "accounts/UserDbCsv.cpp" "accounts/UserDb.cpp" "SystemStats.h")

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Project name deduced to be ${PROJECT_NAME}")

if (POLICY CMP0144)
	cmake_policy(SET CMP0144 NEW)
endif()


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
	PRIVATE
	PkgConfig::gstreamer
	PkgConfig::gstreamer-sdp
	PkgConfig::gstreamer-app
	PkgConfig::gstreamer-video
)

# OpenSSL
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::applink)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Mongoose
if(DEFINED CACHE{MONGOOSE_ROOT})
	message(STATUS "Using MONGOOSE_ROOT from cmake cache: ${MONGOOSE_ROOT}")
	target_include_directories(${PROJECT_NAME} PRIVATE "${MONGOOSE_ROOT}")
elseif(DEFINED ENV{MONGOOSE_ROOT})
	message(STATUS "Using MONGOOSE_ROOT from environment variable: $ENV{MONGOOSE_ROOT}")
	target_include_directories(${PROJECT_NAME} PRIVATE "$ENV{MONGOOSE_ROOT}")
	set(MONGOOSE_ROOT $ENV{MONGOOSE_ROOT})
else()
	message(STATUS "MONGOOSE_ROOT not set!" )
endif()
target_sources (${PROJECT_NAME} PRIVATE "${MONGOOSE_ROOT}/mongoose.h" "${MONGOOSE_ROOT}/mongoose.c")

# target_compile_definitions(${PROJECT_NAME} PUBLIC MG_TLS=MG_TLS_MBED)
target_compile_definitions(${PROJECT_NAME} PUBLIC MG_TLS=MG_TLS_OPENSSL)


# Boost
set(Boost_USE_STATIC_LIBS ON)
find_package( Boost 1.80 REQUIRED COMPONENTS program_options)
target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIR} )
target_link_libraries( ${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES} )

# Spdlog
if(DEFINED CACHE{SPDLOG_ROOT})
	message(STATUS "Using SPDLOG_ROOT from cmake cache: ${SPDLOG_ROOT}")
	list(APPEND CMAKE_PREFIX_PATH ${SPDLOG_ROOT})
elseif(DEFINED ENV{SPDLOG_ROOT})
	message(STATUS "Using SPDLOG_ROOT from environment variable: $ENV{SPDLOG_ROOT}")
	list(APPEND CMAKE_PREFIX_PATH $ENV{SPDLOG_ROOT})
else()
	message(STATUS "SPDLOG_ROOT not set!")
endif()
find_package(spdlog REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

if(WIN32)
	add_compile_definitions(CM_WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE pdh.lib)
elseif(UNIX)
	add_compile_definitions(CM_UNIX)
else()
	add_compile_definitions(CM_OS_UNKNOWN)
endif()

#set(SERVICE_EXECUTABLE_NAME $<TARGET_FILE:PiTvServer>)
set(SERVICE_EXECUTABLE_CMD ${PROJECT_NAME})
message(STATUS "Copying .service file to output directory. SERVICE_EXECUTABLE_CMD is ${SERVICE_EXECUTABLE_CMD}")

configure_file (
    "${PROJECT_SOURCE_DIR}/pitv-server.service.in"
    "${PROJECT_BINARY_DIR}/pitv-server.service"
)