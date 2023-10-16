#pragma once

#include <iostream>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <gst/gst.h>
#include <chrono>
#include <thread>

#include "video/Pipeline.h"
#include "PiTvServer.h"

namespace po = boost::program_options;

bool get_log_fullname(std::string dir_name, std::string filename, bool force_mkdirs, std::string& out_filename)
{
	std::string logging_dir = "";

	if (!dir_name.empty())
	{
		if (!std::filesystem::exists(dir_name))
		{
			if (!force_mkdirs)
			{
				std::cerr << "Directory " << dir_name << " set as logging dir does not exist!" << std::endl;
				return false;
			}
			else
			{
				std::cerr << "Directory " << dir_name << " set as logging dir does not exist. Will be created." << std::endl;
				std::filesystem::create_directories(dir_name);
				logging_dir = dir_name;
			}
		}
		else
		{
			logging_dir = dir_name;
		}
	}

	std::filesystem::path logging_dir_path(logging_dir);
	std::filesystem::path logging_file(filename);
	std::filesystem::path logging_full_path = logging_dir_path / logging_file;
	out_filename = logging_full_path.string();

	return true;
}

std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> create_rotating_log_sink(std::string dir_name, std::string file_name, bool force_mkdirs, int size= 1048576 * 5, int files_num=3)
{
	std::string filepath;
	if (!get_log_fullname(dir_name, file_name, force_mkdirs, filepath))
	{
		return nullptr;
	}
	auto rotating_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filepath, size, files_num);
	return rotating_file_sink;
}

bool setup_logging(std::string dir_name, std::string level, bool force_mkdirs, std::shared_ptr<spdlog::logger>& pipeline_logger_ptr, std::shared_ptr<spdlog::logger>& http_logger_ptr)
{
	// FIXME Not checking for nullptr of create_rotating_log_sink!

	auto general_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto general_filesink = create_rotating_log_sink(dir_name, "pitv-log-general.log", force_mkdirs);
	spdlog::logger general_logger("general", { general_console_sink, general_filesink });
	general_logger.set_pattern("[general] [%^%l%$] %v");

	auto general_logger_ptr = std::make_shared<spdlog::logger>(general_logger);
	spdlog::set_default_logger(general_logger_ptr);

	auto pipeline_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pipeline_filesink = create_rotating_log_sink(dir_name, "pitv-log-pipeline.log", force_mkdirs);
	spdlog::logger pipeline_logger("pipeline", { pipeline_console_sink, pipeline_filesink });
	pipeline_logger.set_pattern("[pipeline] [%^%l%$] %v");
	pipeline_logger_ptr = std::make_shared<spdlog::logger>(pipeline_logger);

	auto http_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto http_filesink = create_rotating_log_sink(dir_name, "pitv-log-http.log", force_mkdirs);
	spdlog::logger http_logger("http", { http_console_sink, http_filesink });
	http_logger.set_pattern("[http] [%^%l%$] %v");
	http_logger_ptr = std::make_shared<spdlog::logger>(http_logger);

	if (!level.empty())
	{
		std::transform(level.begin(), level.end(), level.begin(),
			[](unsigned char c) { return std::tolower(c); }
		);

		spdlog::level::level_enum level_value = spdlog::level::from_str(level);
		spdlog::set_level(level_value);
	}
	else
	{
		// Use SPDLOG_LEVEL
		spdlog::cfg::load_env_levels();
	}

	general_logger_ptr->flush_on(spdlog::level::warn);
	pipeline_logger_ptr->flush_on(spdlog::level::warn);
	http_logger_ptr->flush_on(spdlog::level::warn);

	return true;
}

void print_pipeline_elements_state(GstElement* element, int indent_level, std::stringstream* msg_builder)
{
	std::stringstream message;
	for (int i = 0; i < indent_level; i++)
	{
		message << "\t";
	}

	if (!element)
	{
		spdlog::error("Element was nullptr!");
		message << "NULL";
		return;
	}

	message << GST_ELEMENT_NAME(element);

	GstState state_actual;
	GstState state_pending;

	if (!gst_element_get_state(element, &state_actual, &state_pending, 10 * GST_MSECOND))
	{
		message << " [failed to get state]";
		return;
	}

	message << ": " << gst_element_state_get_name(state_actual);
	if (state_pending != GST_STATE_VOID_PENDING)
	{
		message << " -> " << gst_element_state_get_name(state_pending);
	}

	//spdlog::info(message.str());
	(*msg_builder) << message.str() << std::endl;
}

void log_pipeline_elements_state(Pipeline& pipeline)
{
	std::stringstream elements_status_builder;
	pipeline.traverse_pipeline_elements(
		[&elements_status_builder](GstElement* element, int level)
		{
			print_pipeline_elements_state(element, level, &elements_status_builder);
		}
	);
	spdlog::info(elements_status_builder.str());
}

void populate_listen_addresses(PiTvServerConfig& server_config, const po::variables_map& vm)
{
	if (vm.count("listen"))
	{
		std::vector<std::string> address_vector = vm["listen"].as<std::vector<std::string>>();
		for (auto addr : address_vector)
		{
			if (addr.starts_with("https"))
			{
				server_config.https_listeners.push_back(addr);
			}
			else if (addr.starts_with("http"))
			{
				server_config.http_listeners.push_back(addr);
			}
			else
			{
				spdlog::error("Failed to bind to address {}. Protocol not supported.", addr);
			}
		}
	}
	else
	{
		spdlog::warn("Listening address not specified. Using default address http://0.0.0.0:5000");
		server_config.http_listeners.push_back("http://0.0.0.0:5000");
	}
}

int main(int argc, char** argv)
{
	po::options_description desc("Allowed options");

	desc.add_options()
		("help", "produce help message")
		("log-dir", po::value<std::string>()->default_value("logs"), "logging directory")
		("log-level", po::value<std::string>()->default_value("INFO"), "logging level")
		("force-mkdirs", po::value<bool>()->default_value(true), "create missing directories")
		("listen", po::value<std::vector<std::string>>()->multitoken(), "add listening IP address for HTTP server")
		("camera-dev", po::value<std::string>()->default_value(""), "device to be used as video source")
		("video-width", po::value<int>()->default_value(640), "video width")
		("video-height", po::value<int>()->default_value(640), "video height")
		("video-fps-numerator", po::value<int>()->default_value(20), "video framerate numerator")
		("video-fps-denominator", po::value<int>()->default_value(1), "video framerate denominator")
		("recording-path", po::value<std::string>()->default_value("Recordings"), "path where to store recordings")
		("recording-segment-duration", po::value<int>()->default_value(3600), "duration of a single segment in seconds")
		("recording-max-size", po::value<int>()->default_value(32 * 1024), "Maximum disk space for recordings in Megabytes")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help"))
	{
		std::cout << desc << "\n";
		return 1;
	}

	std::shared_ptr<spdlog::logger> pipeline_logger_ptr;
	std::shared_ptr<spdlog::logger> http_logger_ptr;

	bool force_mkdirs = vm["force-mkdirs"].as<bool>();
	std::string log_level = vm["log-level"].as<std::string>();
	if (!setup_logging(vm["log-dir"].as<std::string>(), log_level, force_mkdirs, pipeline_logger_ptr, http_logger_ptr))
	{
		return 1;
	}
	spdlog::info("Logging set up.");

	gst_init(&argc, &argv);
	
	PipelineConfig pipeline_config;
	pipeline_config.logger_ptr = pipeline_logger_ptr;
	pipeline_config.force_mkdirs = force_mkdirs;
	pipeline_config.recording_path = vm["recording-path"].as<std::string>();
	pipeline_config.camera_dev = vm["camera-dev"].as<std::string>();
	pipeline_config.video_width = vm["video-width"].as<int>();
	pipeline_config.video_height = vm["video-height"].as<int>();
	pipeline_config.video_fps_numerator = vm["video-fps-numerator"].as<int>();
	pipeline_config.video_fps_denominator = vm["video-fps-denominator"].as<int>();
	pipeline_config.recording_segment_duration = vm["recording-segment-duration"].as<int>();
	pipeline_config.recording_max_size = vm["recording-max-size"].as<int>();

	PiTvServerConfig server_config;
	populate_listen_addresses(server_config, vm);
	server_config.logger_ptr = http_logger_ptr;
	server_config.recording_path = vm["recording-path"].as<std::string>();

	Pipeline pipeline(pipeline_config);

	if (!pipeline.construct_pipeline())
	{
		spdlog::error("Failed to construct pipeline!");
		return 1;
	}

	if (!pipeline.start_pipeline())
	{
		spdlog::error("Failed to start pipeline!");
		return 1;
	}

	PiTvServer server(server_config);

	if (!server.start_server())
	{
		spdlog::error("Failed to start http server!");
		return 1;
	}

	while (true)
	{
		pipeline.bus_poll(10);
		server.server_poll(1000);
	}
}