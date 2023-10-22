#include <gst/gst.h>
#include <filesystem>
#include "Pipeline.h"

const std::string Pipeline::recording_extension = "mp4";

void Pipeline::handle_pipeline_message(GstMessage* msg)
{
	GError* err;
	gchar* debug_info;

	switch (GST_MESSAGE_TYPE(msg))
	{
	case GST_MESSAGE_INFO:
		gst_message_parse_info(msg, &err, &debug_info);
		logger()->info("Info from {}: {}\n", GST_OBJECT_NAME(msg->src), err->message);
		logger()->info("Debugging information: {}\n", debug_info ? debug_info : "none");
		g_clear_error(&err);
		g_free(debug_info);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(msg, &err, &debug_info);
		logger()->warn("Warning from {}: {}\n", GST_OBJECT_NAME(msg->src), err->message);
		logger()->warn("Debugging information: {}\n", debug_info ? debug_info : "none");
		g_clear_error(&err);
		g_free(debug_info);
		break;
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, &debug_info);
		logger()->error("Error from {}: {}\n", GST_OBJECT_NAME(msg->src), err->message);
		logger()->error("Debugging information: {}\n", debug_info ? debug_info : "none");
		g_clear_error(&err);
		g_free(debug_info);
		break;
	case GST_MESSAGE_EOS:
		logger()->error("\nEnd-Of-Stream reached!\n");
		break;
	case GST_MESSAGE_STATE_CHANGED:
	{
		GstState old_state, new_state, pending_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

		GstObject* sender = GST_MESSAGE_SRC(msg);

		spdlog::level::level_enum state_change_log_level = spdlog::level::info;

		if (sender != GST_OBJECT(gst_pipeline))
		{
			state_change_log_level = spdlog::level::debug;
		}

		logger()->log(state_change_log_level, "Element {} state changed from {} to {}",
			GST_ELEMENT_NAME(GST_ELEMENT(sender)),
			gst_element_state_get_name(old_state),
			gst_element_state_get_name(new_state)
		);

		log_pipeline_elements_state();

		if (GST_MESSAGE_SRC(msg) == GST_OBJECT(gst_pipeline))
		{
			GstDebugGraphDetails graph_details = static_cast<GstDebugGraphDetails>(
				GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);
			if (new_state == GST_STATE_READY)
			{
				GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(gst_pipeline), graph_details, "pipeline-ready");
			}
			else if (new_state == GST_STATE_PAUSED)
			{
				GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(gst_pipeline), graph_details, "pipeline-paused");
			}
			else if (new_state == GST_STATE_PLAYING)
			{
				GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(gst_pipeline), graph_details, "pipeline-playing");
			}

			is_playing = (new_state == GST_STATE_PLAYING);
		}
	}
	break;
	default:
		/* We should not reach here */
		// g_printerr("Unexpected message received.\n");
		break;
	}
}

const std::string Pipeline::get_current_date_time_str()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[128];
#ifdef _MSC_VER
	localtime_s(&tstruct, &now);
#else
	tstruct = *localtime(&now);
#endif
	strftime(buf, sizeof(buf), "%Y-%m-%d--%H-%M-%S", &tstruct);
	return buf;
}

gchararray Pipeline::format_location_handler(GstElement* splitmux, guint fragment_id, gpointer udata)
{
	assert(udata);
	Pipeline* pipeline = static_cast<Pipeline*>(udata);

	std::string recording_path = pipeline->get_recording_full_path();
	std::string time_str = get_current_date_time_str();

#ifdef __cpp_lib_format
	std::string file_name = std::format("{}[{}].{}", time_str, fragment_id, Pipeline::recording_extension);
#else

	std::stringstream file_name_stream;
	file_name_stream << time_str << "[" << fragment_id << "]" << "." << Pipeline::recording_extension;
	std::string file_name = file_name_stream.str();
#endif

	std::filesystem::path dir_path(recording_path);
	std::filesystem::path file(file_name);
	std::filesystem::path full_path = dir_path / file;
	std::string path_str = full_path.string();

	pipeline->config.logger_ptr->info("Recording fragment will be saved to {}", path_str);

	pipeline->enforce_recording_max_size_restrictions(path_str, fragment_id);

	gchar* file_path_dup = g_strdup(path_str.c_str());
	return file_path_dup;
}

uintmax_t Pipeline::get_recording_total_size() const
{
	uintmax_t total_size = 0;

	for (auto const& dir_entry : std::filesystem::directory_iterator{ get_recording_full_path() })
	{
		if (!dir_entry.is_regular_file())
		{
			logger()->debug("[get_recording_total_size] entry {} ignored due to not being a file", dir_entry.path().string());
			continue;
		}

		if (dir_entry.path().extension() != "." + recording_extension)
		{
			logger()->debug("[get_recording_total_size] entry {} ignored due to not being a .{} video container", dir_entry.path().string(), recording_extension);
			continue;
		}

		auto size = dir_entry.file_size();
		logger()->debug("[get_recording_total_size] file {} size is {} Mb", dir_entry.path().string(), size / 1024 / 1024);
		total_size += dir_entry.file_size();
	}
	return total_size;
}

std::filesystem::path Pipeline::get_oldest_file() const
{
	bool has_file = false;
	std::filesystem::path oldest_file;
	std::filesystem::file_time_type last_write_time;

	for (auto const& dir_entry : std::filesystem::directory_iterator{ get_recording_full_path() })
	{
		if (!dir_entry.is_regular_file())
		{
			logger()->debug("[get_oldest_file] entry {} ignored due to not being a file", dir_entry.path().string());
			continue;
		}

		if (dir_entry.path().extension() != "." + recording_extension)
		{
			logger()->debug("[get_oldest_file] entry {} ignored due to not being a .{} video container", dir_entry.path().string(), recording_extension);
			continue;
		}

		auto last_write_time_tmp = dir_entry.last_write_time();
		if (last_write_time_tmp < last_write_time || !has_file)
		{
			last_write_time = last_write_time_tmp;
			oldest_file = dir_entry.path();
			has_file = true;
		}
	}

	return oldest_file;
}

void Pipeline::enforce_recording_max_size_restrictions(std::filesystem::path last_fragment_path, int last_fragment_index)
{
	if (config.recording_max_size <= 0)
	{
		logger()->warn("recording_max_size set to zero or a negative number! The storage will grow indefinitely untill it runs out of disk space!");
	}

	int max_iterations = 100;
	int iteration = 0;

	uintmax_t total_size = get_recording_total_size();
	logger()->info("[enforce_recording_max_size_restrictions] total size of recordings: {} Mb", total_size / 1024 / 1024);

	while(total_size >= (uintmax_t)config.recording_max_size * 1024UL * 1024UL && iteration < max_iterations)
	{
		std::filesystem::path oldest_file = get_oldest_file();
		if (oldest_file.empty())
		{
			logger()->error("[enforce_recording_max_size_restrictions] max recording size reached, but oldest file was not found!");
			return;
		}

		if (oldest_file == last_fragment_path)
		{
			logger()->error("[enforce_recording_max_size_restrictions] max recording size reached, but oldest file is the current one!");
			return;
		}

		logger()->info("[enforce_recording_max_size_restrictions] removing the oldest file: {}", oldest_file.string());
		std::filesystem::remove(oldest_file);

		total_size = get_recording_total_size();
		iteration++;
	}

	if (iteration >= max_iterations)
	{
		logger()->error("[enforce_recording_max_size_restrictions] loop exited due to max_iterations reached!");
	}

	logger()->info("[enforce_recording_max_size_restrictions] total size of recordings after cleaning: {} Mb", total_size / 1024 / 1024);
}

void Pipeline::bus_poll(int timeout_msec)
{
	if (!bus)
	{
		logger()->error("Cannot poll buss, it is nullptr!");
		return;
	}

	logger()->debug("Polling bus {} for messages", GST_ELEMENT_NAME(bus));
	GstMessage* message = gst_bus_timed_pop_filtered(bus, timeout_msec * GST_MSECOND,
		(GstMessageType)(
			GST_MESSAGE_INFO |
			GST_MESSAGE_WARNING |
			GST_MESSAGE_ERROR |
			GST_MESSAGE_EOS | 
			GST_MESSAGE_STATE_CHANGED));

	if (message)
	{
		handle_pipeline_message(message);
	}
}

bool Pipeline::set_recording_full_path()
{
	std::string recordings_path;
	if (std::filesystem::exists(config.recording_path))
	{
		recordings_path = config.recording_path;
	}
	else if (config.force_mkdirs)
	{
		config.logger_ptr->warn("[format_location_handler] directory '{}' does not exist, will be created", config.recording_path);
		std::filesystem::create_directories(config.recording_path);
	}
	else
	{
		config.logger_ptr->error("[format_location_handler] directory '{}' does not exist!", config.recording_path);
		return false;
	}
	std::filesystem::path dir_path(recordings_path);
	recording_full_path = dir_path.string();
	return true;
}

std::string Pipeline::get_recording_full_path() const
{
	return recording_full_path;
}

GstElement* Pipeline::make_streaming_subpipe()
{
	std::string bin_name = std::string("rtp-bin");
	GstElement* bin = gst_bin_new(bin_name.c_str());
	assert(bin);

	std::string rtph264pay_name = std::string("rtph264pay");
	GstElement* rtph264pay = gst_element_factory_make("rtph264pay", rtph264pay_name.c_str());
	assert(rtph264pay);

	GstElement* multiudpsink = gst_element_factory_make("multiudpsink", "multiudpsink");
	assert(multiudpsink);

	gst_bin_add_many(GST_BIN(bin), rtph264pay, multiudpsink, NULL);

	gboolean link_ok = gst_element_link(rtph264pay, multiudpsink);
	assert(link_ok);

	GstPad* rtph264pay_sink = gst_element_get_static_pad(rtph264pay, "sink");
	GstPad* sink_ghost = gst_ghost_pad_new("sink", rtph264pay_sink);
	gst_element_add_pad(bin, sink_ghost);
	gst_object_unref(rtph264pay_sink);

	return bin;
}

bool Pipeline::attach_rtp_bin(GstElement* element)
{
	assert(element);

	if (!gst_pipeline)
	{
		logger()->error("Failed to attach bin {} to not constructed pipeline!", GST_ELEMENT_NAME(element));
		return false;
	}

	logger()->info("Trying to attach element {} to pipeline {}", GST_ELEMENT_NAME(element), GST_ELEMENT_NAME(gst_pipeline));

	if (gst_element_get_parent(element))
	{
		logger()->error("Failed to attach bin {} to pipeline {}, because the bin already has a parent!",
			GST_ELEMENT_NAME(element),
			GST_ELEMENT_NAME(gst_pipeline),
			"subpipes_tee");
		return false;
	}

	GstElement* tee = gst_bin_get_by_name(GST_BIN(gst_pipeline), "subpipes_tee");
	if (!tee)
	{
		logger()->error("Failed to attach bin {} to pipeline {}, because tee with name '{}' was not found!", 
			GST_ELEMENT_NAME(element),
			GST_ELEMENT_NAME(gst_pipeline),
			"subpipes_tee");
		return false;
	}

	if (!gst_bin_add(GST_BIN(gst_pipeline), element))
	{
		logger()->error("Failed to add bin {} to pipeline {}!",
			GST_ELEMENT_NAME(element),
			GST_ELEMENT_NAME(gst_pipeline));
		return false;
	}

	if (!gst_element_link(tee, element))
	{
		logger()->error("Failed to link bin {} to tee {}!",
			GST_ELEMENT_NAME(element),
			GST_ELEMENT_NAME(tee));
		gst_bin_remove(GST_BIN(gst_pipeline), element);
		return false;
	}

	logger()->info("Trying to sync {}'s state with the parent pipeline...", GST_ELEMENT_NAME(element));
	
	if (!gst_element_sync_state_with_parent(element))
	{
		logger()->error("Failed to sync {}'s state with parent!",
			GST_ELEMENT_NAME(element));
		gst_bin_remove(GST_BIN(gst_pipeline), element);
		return false;
	}

	if (!gst_bin_sync_children_states(GST_BIN(element)))
	{
		logger()->error("Failed to sync {}'s children states!",
			GST_ELEMENT_NAME(element));
		gst_bin_remove(GST_BIN(gst_pipeline), element);
		return false;
	}
	
	std::string dot_name = std::string("pipeline-attached");
	GstDebugGraphDetails graph_details = static_cast<GstDebugGraphDetails>(
		GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);
	GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(gst_pipeline), graph_details, dot_name.c_str());


	logger()->info("Bin {} was successfully attached to pipeline {}!", GST_ELEMENT_NAME(element), GST_ELEMENT_NAME(gst_pipeline));
	return true;
}

bool Pipeline::detach_rtp_bin(GstElement* bin)
{
	if (!gst_pipeline)
	{
		return false;
	}

	if (!gst_bin_remove(GST_BIN(gst_pipeline), bin))
	{
		logger()->error("Failed to remove {} from pipeline {}!",
			GST_ELEMENT_NAME(bin),
			GST_ELEMENT_NAME(gst_pipeline));
		return false;
	}

	if (gst_element_set_state(bin, GstState::GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
	{
		logger()->error("Failed to set bin state to NULL!");
	}

	if (!gst_bin_sync_children_states(GST_BIN(bin)))
	{
		logger()->error("Failed to sync {}'s children states!",
			GST_ELEMENT_NAME(bin));
		return false;
	}

	logger()->info("Bin {} was successfully detached from pipeline {}!", GST_ELEMENT_NAME(bin), GST_ELEMENT_NAME(gst_pipeline));

	std::string dot_name = std::string("pipeline-detached");
	GstDebugGraphDetails graph_details = static_cast<GstDebugGraphDetails>(
		GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);
	GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(gst_pipeline), graph_details, dot_name.c_str());

	gst_object_unref(bin);

	return true;
}

void Pipeline::dump_pipeline_dot(std::string name) const
{
	GstDebugGraphDetails graph_details = static_cast<GstDebugGraphDetails>(
		GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);
	GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(gst_pipeline), graph_details, name.c_str());
}

bool Pipeline::rtp_add_endpoint(std::string host, int port)
{
	if (!gst_pipeline)
	{
		logger()->error("rtp_add_endpoint() called for not constructed pipeline!");
		return false;
	}

	GstElement* multiudpsink = gst_bin_get_by_name(GST_BIN(gst_pipeline), "multiudpsink");
	if (!multiudpsink)
	{
		logger()->error("rtp_add_endpoint() failed to find multiudpsink!");
		return false;
	}

	g_signal_emit_by_name(multiudpsink, "add", host.c_str(), port);
	logger()->info("Signal sent to add {}:{} endpoint to the RTP subpipe", host, port);
	return true;
}

bool Pipeline::rtp_remove_endpoint(std::string host, int port)
{
	if (!gst_pipeline)
	{
		logger()->error("rtp_remove_endpoint() called for not constructed pipeline!");
		return false;
	}

	GstElement* multiudpsink = gst_bin_get_by_name(GST_BIN(gst_pipeline), "multiudpsink");
	if (!multiudpsink)
	{
		logger()->error("rtp_add_endpoint() failed to find multiudpsink!");
		return false;
	}

	g_signal_emit_by_name(multiudpsink, "remove", host.c_str(), port);

	logger()->info("Signal sent to remove {}:{} endpoint from the RTP subpipe", host, port);
	return true;
}

bool Pipeline::rtp_change_endpoint(std::string host_old, int port_old, std::string host, int port)
{
	rtp_add_endpoint(host, port);
	rtp_remove_endpoint(host_old, port_old);
	return true;
}

bool Pipeline::splitmux_split_after()
{
	if (!gst_pipeline)
	{
		logger()->error("splitmux_split_after() called for not constructed pipeline!");
		return false;
	}

	GstElement* splitmux = gst_bin_get_by_name(GST_BIN(gst_pipeline), "splitmuxsink");
	if (!splitmux)
	{
		logger()->error("splitmux_split_after() failed to find the splitmuxsink in pipeline {}!", GST_ELEMENT_NAME(gst_pipeline));
		return false;
	}

	g_signal_emit_by_name(splitmux, "split-after");
	logger()->info("split-after called!");
	return true;
}

Pipeline::Pipeline(const PipelineConfig& config)
{
	this->config = config;
}

Pipeline::~Pipeline()
{
	if (gst_pipeline)
	{
		GstState state = GstState::GST_STATE_NULL;
		GstStateChangeReturn state_ret = gst_element_get_state(gst_pipeline, &state, NULL, 0);
		if (state_ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
		{
			config.logger_ptr->error("[~Pipeline] Failed to get pipeline's state");
		}
		else if (state != GstState::GST_STATE_NULL)
		{
			logger()->info("[~Pipeline] Setting pipeline to NULL state...");
			GstStateChangeReturn ret = gst_element_set_state(gst_pipeline, GstState::GST_STATE_NULL);
			if (ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
			{
				logger()->error("[~Pipeline] Failed to set pipeline to NULL state!");
			}
			else
			{
				logger()->info("[~Pipeline] Pipeline set to NULL state successfully!");
			}
		}

		gst_object_unref(gst_pipeline);
	}

	if (bus)
	{
		gst_object_unref(bus);
	}
}

std::shared_ptr<spdlog::logger> Pipeline::logger() const
{
	return config.logger_ptr;
}

GstElement* Pipeline::make_capturing_subpipe()
{
#ifdef CM_UNIX
	GstElement* bin = gst_bin_new("video-source-bin");
	assert(bin);

	GstElement* source = gst_element_factory_make("libcamerasrc", "libcamerasrc");
	if (!source)
	{
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), source);

	GstElement* encoder = gst_element_factory_make("v4l2h264enc", "v4l2h264enc");
	if (!encoder)
	{
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), encoder);

	GstStructure* encoder_extra = gst_structure_new("encoder_extra_controls",
		"repeat_sequence_header", G_TYPE_INT, 1,
		NULL
	);

	g_object_set(encoder, "extra-controls", encoder_extra, NULL);

	GstElement* encoder_capsfilter = gst_element_factory_make("capsfilter", "encoder_caps");
	if (!encoder_capsfilter)
	{
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), encoder_capsfilter);

	GstCaps* source_caps = gst_caps_new_simple("video/x-raw",
		"width", G_TYPE_INT, config.video_width,
		"height", G_TYPE_INT, config.video_height,
		"framerate", GST_TYPE_FRACTION, config.video_fps_numerator, config.video_fps_denominator,
		"format", G_TYPE_STRING, "YUY2",
		"interlace-mode", G_TYPE_STRING, "progressive",

		NULL);

	if (!gst_element_link_filtered(source, encoder, source_caps))
	{
		GST_ERROR("Failed to link %s and %s!", GST_ELEMENT_NAME(source), GST_ELEMENT_NAME(encoder));
		gst_object_unref(bin);
		return nullptr;
	}
	gst_caps_unref(source_caps);

	GstCaps* encoder_caps = gst_caps_new_simple(
		"video/x-h264",
		"level", G_TYPE_STRING, "3.1",
		// "profile", G_TYPE_STRING, "constrained-baseline",
		NULL);
	g_object_set(encoder_capsfilter, "caps", encoder_caps, NULL);
	if (!gst_element_link(encoder, encoder_capsfilter))
	{
		GST_ERROR("Failed to link %s and %s!", GST_ELEMENT_NAME(encoder), GST_ELEMENT_NAME(encoder_capsfilter));
		gst_object_unref(bin);
		return nullptr;
	}
	gst_caps_unref(encoder_caps);

	GstPad* encoder_capsfilter_src_pad = gst_element_get_static_pad(encoder_capsfilter, "src");
	GstPad* src_ghost_pad = gst_ghost_pad_new("src", encoder_capsfilter_src_pad);

	gst_element_add_pad(bin, src_ghost_pad);
	g_object_unref(encoder_capsfilter_src_pad);

	return bin;

#elif CM_WIN32

	GstElement* bin = gst_bin_new("video-source-bin");
	assert(bin);

	// GstElement* source = gst_element_factory_make("mfvideosrc", "mfvideosrc");
	GstElement* source = gst_element_factory_make("videotestsrc", "videotestsrc");
	if (!source)
	{
		logger()->error("Failed to create source element!");
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), source);
	g_object_set(source, "is-live", true, NULL);

	/*
	GstElement* encoder = gst_element_factory_make("mfh264enc", "mfh264enc");
	if (!encoder)
	{
		logger()->error("Failed to create mfh264enc element!");
		gst_object_unref(bin);
		return nullptr;
	}
	g_object_set(encoder, "low-latency", TRUE, NULL);
	*/
	// xh264
	GstElement* encoder = gst_element_factory_make("x264enc", "x264enc");
	if (!encoder)
	{
		logger()->error("Failed to create x264enc element!");
		gst_object_unref(bin);
		return nullptr;
	}
	g_object_set(encoder, "tune", 4, NULL);
	gst_bin_add(GST_BIN(bin), encoder);
	

	GstCaps* source_caps = gst_caps_new_simple("video/x-raw",
		"width", G_TYPE_INT, config.video_width,
		"height", G_TYPE_INT, config.video_height,
		"framerate", GST_TYPE_FRACTION, config.video_fps_numerator, config.video_fps_denominator,
		"format", G_TYPE_STRING, "NV12",
		NULL);

	if (!gst_element_link_filtered(source, encoder, source_caps))
	{
		logger()->error("Failed to link video source elements!");
		return nullptr;
	}

	GstPad* encoder_src_pad = gst_element_get_static_pad(encoder, "src");
	GstPad* src_ghost_pad = gst_ghost_pad_new("src", encoder_src_pad);

	gst_element_add_pad(bin, src_ghost_pad);
	g_object_unref(encoder_src_pad);
	
	return bin;

#else
#error OS not supported!
	return nullptr;
#endif

}

GstElement* Pipeline::make_recording_subpipe()
{
	GstElement* bin = gst_bin_new("recording-bin");
	GstElement* storing_queue = gst_element_factory_make("queue", "storing_queue");
	if (!storing_queue)
	{
		logger()->error("Failed to create storing queue!");
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), storing_queue);

	GstElement* parser = gst_element_factory_make("h264parse", "h264parse");
	if (!parser)
	{
		logger()->error("Failed to create h264parse element!");
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), parser);

	if (!gst_element_link(storing_queue, parser))
	{
		logger()->error("Failed to link storing_queue and h264parse!");
		gst_object_unref(bin);
		return nullptr;
	}

	GstElement* sink = gst_element_factory_make("splitmuxsink", "splitmuxsink");
	if (!sink)
	{
		logger()->error("Failed to create splitmuxsink element!");
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), sink);

	g_object_set(sink, "max-size-time", config.recording_segment_duration * GST_SECOND, NULL);
	g_object_set(sink, "async-finalize", false, NULL);
	// g_object_set(sink, "location", pipeline_data.config.recording_path, NULL);
	g_signal_connect(sink, "format-location", G_CALLBACK(&Pipeline::format_location_handler), this);

	GstPad* splitmuxsink_sink_pad;
#if GST_VERSION_MAJOR >= 1 && GST_VERSION_MINOR >= 20 && 0
	splitmuxsink_sink_pad = gst_element_request_pad_simple(sink, "video");
#else

	GstPadTemplate* sink_pad_template = gst_element_get_pad_template(sink, "video");
	GstCaps* sink_pad_caps = gst_pad_template_get_caps(sink_pad_template);
	splitmuxsink_sink_pad = gst_element_request_pad(sink, sink_pad_template, "video", sink_pad_caps);
	gst_caps_unref(sink_pad_caps);
	gst_object_unref(sink_pad_template);
	if (!splitmuxsink_sink_pad)
	{
		logger()->error("Failed to create sink pad!");
		return nullptr;
	}

	GstPad* parser_src_pad = gst_element_get_static_pad(parser, "src");
	GstPadLinkReturn ret = gst_pad_link(parser_src_pad, splitmuxsink_sink_pad);
	assert(ret == GstPadLinkReturn::GST_PAD_LINK_OK);
	g_object_unref(parser_src_pad);

	GstPad* queue_sink_pad = gst_element_get_static_pad(storing_queue, "sink");
	GstPad* sink_ghost_pad = gst_ghost_pad_new("recording-sink", queue_sink_pad);
	gst_element_add_pad(bin, sink_ghost_pad);
	gst_object_unref(queue_sink_pad);
#endif

	return bin;
}

bool Pipeline::construct_pipeline()
{
	if (gst_pipeline)
	{
		logger()->warn("Calling construct_pipeline for an already constructed pipeline!");
		return true;
	}

	GstElement* pipeline_tmp = gst_pipeline_new("main-pipeline");
	assert(pipeline_tmp);

	GstElement* video_capturing_bin = make_capturing_subpipe();

	if (!video_capturing_bin)
	{
		logger()->error("Failed to create video source!");
		gst_object_unref(pipeline_tmp);
		return false;
	}

	gst_bin_add(GST_BIN(pipeline_tmp), video_capturing_bin);

	GstElement* subpipes_tee = gst_element_factory_make("tee", "subpipes_tee");
	if (!subpipes_tee)
	{
		logger()->error("Failed to create subpipes_tee element!");
		gst_object_unref(pipeline_tmp);
		return false;
	}
	gst_bin_add(GST_BIN(pipeline_tmp), subpipes_tee);

	GstElement* recording_bin = make_recording_subpipe();
	if (!recording_bin)
	{
		logger()->error("Failed to create recording subpipeline!");
		gst_object_unref(pipeline_tmp);
		return false;
	}
	gst_bin_add(GST_BIN(pipeline_tmp), recording_bin);
	
	if (!gst_element_link_many(video_capturing_bin, subpipes_tee, recording_bin, NULL))
	{
		logger()->error("Failed to link pipeline elements!");
		gst_object_unref(pipeline_tmp);
		return false;
	}

	GstElement* rtp_bin = make_streaming_subpipe();
	if (!rtp_bin)
	{
		logger()->error("Failed to create rtp_bin!");
		gst_object_unref(pipeline_tmp);
		return false;
	}
	gst_bin_add(GST_BIN(pipeline_tmp), rtp_bin);

	if (!gst_element_link(subpipes_tee, rtp_bin))
	{
		logger()->error("Failed to link bin {} to tee {}!",
			GST_ELEMENT_NAME(rtp_bin),
			GST_ELEMENT_NAME(subpipes_tee));
		gst_bin_remove(GST_BIN(gst_pipeline), rtp_bin);
		return false;
	}

	GstDebugGraphDetails graph_details = static_cast<GstDebugGraphDetails>(
		GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);
	GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_tmp), graph_details, "pipeline-start");

	gst_pipeline = pipeline_tmp;
	bus = gst_element_get_bus(pipeline_tmp);
	assert(bus);

	return true;
}

bool Pipeline::start_pipeline()
{
	if (!set_recording_full_path())
	{
		logger()->error("Cannot start pipeline because recording directory does not exist!");
		return false;
	}

	if (!gst_pipeline)
	{
		logger()->error("start_pipeline() called for not constructed pipeline!");
		return false;
	}

	if (is_pipeline_running())
	{
		logger()->warn("start_pipeline() called for already running pipeline!");
		return true;
	}

	GstStateChangeReturn set_state_code = gst_element_set_state(gst_pipeline, GST_STATE_PLAYING);

	if (set_state_code == GST_STATE_CHANGE_FAILURE)
	{
		logger()->error("Failed to set the pipeline to the playing state!\n");
		return false;
	}

	logger()->info("Pipeline successfully set to PLAYING state with result code: {}", set_state_code);

	return true;
}

bool Pipeline::pause_pipeline()
{
	if (!gst_pipeline)
	{
		logger()->error("pause_pipeline() called for not contrucred pipeline!");
		return false;
	}

	if (!is_pipeline_running())
	{
		logger()->warn("pause_pipeline() called for not running pipeline!");
		return true;
	}

	GstStateChangeReturn set_state_code = gst_element_set_state(gst_pipeline, GST_STATE_PAUSED);

	if (set_state_code == GST_STATE_CHANGE_FAILURE)
	{
		logger()->error("Failed to set the pipeline to the playing state!\n");
		return false;
	}

	logger()->info("Pipeline successfully set to PAUSED state with result code: %d", set_state_code);
	return true;
}

bool Pipeline::is_pipeline_running() const
{
	if (!gst_pipeline)
	{
		return false;
	}

	GstState state;
	GstStateChangeReturn state_ret = gst_element_get_state(gst_pipeline, &state, NULL, 3 * GST_SECOND);
	if (state_ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
	{
		logger()->error("is_pipeline_running() could not retrieve pipeline state!");
		return false;
	}
	return state == GstState::GST_STATE_PLAYING;
}

void Pipeline::print_pipeline_elements_state(GstElement* element, int indent_level, std::stringstream* msg_builder)
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

void Pipeline::log_pipeline_elements_state() const
{
	if (logger()->level() < spdlog::level::debug)
	{
		return;
	}

	std::stringstream elements_status_builder;
	traverse_pipeline_elements(
		[&elements_status_builder](GstElement* element, int level)
		{
			print_pipeline_elements_state(element, level, &elements_status_builder);
		}
	);
	
	logger()->debug(elements_status_builder.str());
}