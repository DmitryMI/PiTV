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

#include "video/Pipeline.h"
#include "PiTvServer.h"

namespace po = boost::program_options;

bool setup_logging(std::string dir_name, bool force_mkdirs, std::shared_ptr<spdlog::logger>& pipeline_logger_ptr, std::shared_ptr<spdlog::logger>& http_logger_ptr)
{
	spdlog::cfg::load_env_levels();

	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_level(spdlog::level::warn);
	console_sink->set_pattern("[general] [%^%l%$] %v");

	std::string logging_dir = "";
	std::string logging_file_basename = "general.txt";
	if (!dir_name.empty())
	{
		if (!std::filesystem::exists(dir_name))
		{
			std::cerr << "Directory " << dir_name << " set as logging dir does not exist!" << std::endl;
			return false;
		}
		else if (force_mkdirs)
		{
			std::cerr << "Directory " << dir_name << " set as logging dir does not exist. Will be created." << std::endl;
			std::filesystem::create_directories(dir_name);
			logging_dir = dir_name;
		}
		else
		{
			logging_dir = dir_name;
		}
	}

	std::filesystem::path logging_dir_path(logging_dir);
	std::filesystem::path logging_file(logging_file_basename);
	std::filesystem::path logging_full_path = logging_dir_path / logging_file;
	logging_file_basename = logging_full_path.string();

	auto rotating_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logging_file_basename, 1048576 * 5, 3);
	spdlog::logger logger("general", { console_sink, rotating_file_sink });
	auto logger_ptr = std::make_shared<spdlog::logger>(logger);
	spdlog::set_default_logger(logger_ptr);

	spdlog::logger pipeline_logger("pipeline", { console_sink, rotating_file_sink });
	pipeline_logger_ptr = std::make_shared<spdlog::logger>(pipeline_logger);

	spdlog::logger http_logger("http", { console_sink, rotating_file_sink });
	http_logger_ptr = std::make_shared<spdlog::logger>(http_logger);
}

int main(int argc, char** argv)
{
	po::options_description desc("Allowed options");

	desc.add_options()
		("help", "produce help message")
		("log-dir", po::value<std::string>()->default_value("logs"), "logging directory")
		("force-mkdirs", po::value<bool>()->default_value(true), "create missing directories")
		("listen", po::value<std::string>()->default_value("http://0.0.0.0:5000"), "add listening IP address for HTTP server")
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
	if (!setup_logging(vm["log-dir"].as<std::string>(), force_mkdirs, pipeline_logger_ptr, http_logger_ptr))
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

	while (true)
	{
		pipeline.bus_poll(100);
	}
}