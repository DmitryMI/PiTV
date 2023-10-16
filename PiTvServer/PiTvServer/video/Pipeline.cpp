#include <gst/gst.h>
#include <filesystem>
#include "Pipeline.h"

void Pipeline::handle_pipeline_message(GstMessage* msg)
{
	GError* err;
	gchar* debug_info;

	switch (GST_MESSAGE_TYPE(msg))
	{
	case GST_MESSAGE_INFO:
		gst_message_parse_error(msg, &err, &debug_info);
		logger()->info("Info from {}: {}\n", GST_OBJECT_NAME(msg->src), err->message);
		logger()->info("Debugging information: {}\n", debug_info ? debug_info : "none");
		g_clear_error(&err);
		g_free(debug_info);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_error(msg, &err, &debug_info);
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

		if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_data.pipeline))
		{
			logger()->info("Pipeline state changed from {} to {}:\n",
				gst_element_state_get_name(old_state),
				gst_element_state_get_name(new_state)
			);

			GstDebugGraphDetails graph_details = static_cast<GstDebugGraphDetails>(
				GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);
			if (new_state == GST_STATE_READY)
			{
				GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_data.pipeline), graph_details, "pipeline-ready");
			}
			else if (new_state == GST_STATE_PAUSED)
			{
				GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_data.pipeline), graph_details, "pipeline-paused");
			}
			else if (new_state == GST_STATE_PLAYING)
			{
				GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_data.pipeline), graph_details, "pipeline-playing");
			}

			pipeline_data.is_playing = (new_state == GST_STATE_PLAYING);
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
	PipelineData* pipeline_data = static_cast<PipelineData*>(udata);
	std::string recordings_path;
	if (std::filesystem::exists(pipeline_data->config.recording_path))
	{
		recordings_path = pipeline_data->config.recording_path;
	}
	else if (pipeline_data->config.force_mkdirs)
	{
		pipeline_data->config.logger_ptr->warn("[format_location_handler] directory '{}' does not exist, will be created", pipeline_data->config.recording_path);
		std::filesystem::create_directories(pipeline_data->config.recording_path);
	}
	else
	{
		pipeline_data->config.logger_ptr->error("[format_location_handler] directory '{}' does not exist!", pipeline_data->config.recording_path);
		recordings_path = "";
	}

	std::string time_str = get_current_date_time_str();

#ifdef __cpp_lib_format
	std::string file_name = std::format("{}[{}].mp4", recordings_path, time_str, fragment_id);
#else

	std::stringstream file_name_stream;
	file_name_stream << time_str << "[" << fragment_id << "]" << ".mp4";
	std::string file_name = file_name_stream.str();
#endif

	std::filesystem::path dir_path(recordings_path);
	std::filesystem::path file(file_name);
	std::filesystem::path full_path = dir_path / file;
	std::string path_str = full_path.string();

	gchar* file_path_dup = g_strdup(path_str.c_str());
	return file_path_dup;
}

void Pipeline::bus_poll(int timeout_msec)
{
	GstMessage* message = gst_bus_timed_pop_filtered(pipeline_data.bus, timeout_msec * GST_MSECOND,
		(GstMessageType)(
			GST_MESSAGE_ERROR |
			GST_MESSAGE_EOS | 
			GST_MESSAGE_STATE_CHANGED));
	if (message)
	{
		handle_pipeline_message(message);
	}
}

Pipeline::Pipeline(const PipelineConfig& config)
{
	this->pipeline_data.config = config;
}

Pipeline::~Pipeline()
{
	if (pipeline_data.pipeline)
	{
		GstState state = GstState::GST_STATE_NULL;
		GstStateChangeReturn state_ret = gst_element_get_state(pipeline_data.pipeline, &state, NULL, 0);
		if (state_ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
		{
			pipeline_data.config.logger_ptr->error("[~Pipeline] Failed to get pipeline's state");
		}
		else if (state != GstState::GST_STATE_NULL)
		{
			logger()->info("[~Pipeline] Setting pipeline to NULL state...");
			GstStateChangeReturn ret = gst_element_set_state(pipeline_data.pipeline, GstState::GST_STATE_NULL);
			if (ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
			{
				logger()->error("[~Pipeline] Failed to set pipeline to NULL state!");
			}
			else
			{
				logger()->info("[~Pipeline] Pipeline set to NULL state successfully!");
			}
		}

		gst_object_unref(pipeline_data.pipeline);
	}

	if (pipeline_data.bus)
	{
		gst_object_unref(pipeline_data.bus);
	}
}

std::shared_ptr<spdlog::logger> Pipeline::logger() const
{
	return pipeline_data.config.logger_ptr;
}

GstElement* Pipeline::make_capturing_subpipe()
{
#ifdef CM_UNIX

	GstElement* source = gst_element_factory_make("libcamerasrc", "libcamerasrc");
	if (!source)
	{
		return false;
	}

	GstElement* encoder = gst_element_factory_make("v4l2h264enc", "v4l2h264enc");
	if (!encoder)
	{
		gst_object_unref(source);
		return false;
	}

	GstStructure* encoder_extra = gst_structure_new("encoder_extra_controls",
		"repeat_sequence_header", G_TYPE_INT, 1,
		NULL
	);

	g_object_set(encoder, "extra-controls", encoder_extra, NULL);
	// gst_video_encoder_set_latency(GST_VIDEO_ENCODER(encoder), 0 * GST_MSECOND, 100 * GST_MSECOND);

	GstElement* encoder_capsfilter = gst_element_factory_make("capsfilter", "encoder_caps");
	if (!encoder_capsfilter)
	{
		gst_object_unref(source);
		gst_object_unref(encoder);
		return false;
	}

	gst_bin_add_many(GST_BIN(pipeline), source, encoder, encoder_capsfilter, NULL);

	GstCaps* source_caps = gst_caps_new_simple("video/x-raw",
		"width", G_TYPE_INT, data.video_width,
		"height", G_TYPE_INT, data.video_height,
		"framerate", GST_TYPE_FRACTION, data.framerate_high, data.framerate_low,
		"format", G_TYPE_STRING, "YUY2",
		"interlace-mode", G_TYPE_STRING, "progressive",

		NULL);

	if (!gst_element_link_filtered(source, encoder, source_caps))
	{
		GST_ERROR("Failed to link %s and %s!", GST_ELEMENT_NAME(source), GST_ELEMENT_NAME(encoder));
		return false;
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
		return false;
	}
	gst_caps_unref(encoder_caps);

	out_src_element = encoder_capsfilter;

	return true;

#elif CM_WIN32

	GstElement* bin = gst_bin_new("video-source-bin");
	assert(bin);

	GstElement* source = gst_element_factory_make("mfvideosrc", "mfvideosrc");
	if (!source)
	{
		logger()->error("Failed to create source element!");
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), source);

	GstElement* encoder = gst_element_factory_make("mfh264enc", "mfh264enc");
	if (!encoder)
	{
		logger()->error("Failed to create mfh264enc element!");
		gst_object_unref(bin);
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), encoder);
	g_object_set(encoder, "low-latency", TRUE, NULL);

	GstCaps* source_caps = gst_caps_new_simple("video/x-raw",
		"width", G_TYPE_INT, pipeline_data.config.video_width,
		"height", G_TYPE_INT, pipeline_data.config.video_height,
		"framerate", GST_TYPE_FRACTION, pipeline_data.config.video_fps_numerator, pipeline_data.config.video_fps_denominator,
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
		GST_ERROR("Failed to create storing queue!");
		return nullptr;
	}
	gst_bin_add(GST_BIN(bin), storing_queue);

	GstElement* parser = gst_element_factory_make("h264parse", "h264parse");
	if (!parser)
	{
		logger()->error("Failed to create splitmuxsink element!");
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

	g_object_set(sink, "max-size-time", pipeline_data.config.recording_segment_duration * GST_SECOND, NULL);
	g_object_set(sink, "async-finalize", false, NULL);
	g_object_set(sink, "location", pipeline_data.config.recording_path, NULL);
	g_signal_connect(sink, "format-location", G_CALLBACK(format_location_handler), &pipeline_data);

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
	if (pipeline_data.pipeline)
	{
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

	GstDebugGraphDetails graph_details = static_cast<GstDebugGraphDetails>(
		GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS);
	GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_tmp), graph_details, "pipeline-start");

	pipeline_data.pipeline = pipeline_tmp;
	pipeline_data.bus = gst_element_get_bus(pipeline_tmp);
	assert(pipeline_data.bus);

	return true;
}

bool Pipeline::start_pipeline()
{
	if (!pipeline_data.pipeline)
	{
		logger()->error("start_pipeline() called for not contrucred pipeline!");
		return false;
	}

	if (is_pipeline_running())
	{
		logger()->warn("start_pipeline() called for already running pipeline!");
		return true;
	}

	GstStateChangeReturn set_state_code = gst_element_set_state(pipeline_data.pipeline, GST_STATE_PLAYING);

	if (set_state_code == GST_STATE_CHANGE_FAILURE)
	{
		logger()->error("Failed to set the pipeline to the playing state!\n");
		return false;
	}

	logger()->info("Pipeline successfully set to PLAYING state with result code: %d", set_state_code);
}

bool Pipeline::pause_pipeline()
{
	if (!pipeline_data.pipeline)
	{
		return false;
	}

	if (!pipeline_data.pipeline)
	{
		logger()->error("pause_pipeline() called for not contrucred pipeline!");
		return false;
	}

	if (!is_pipeline_running())
	{
		logger()->warn("pause_pipeline() called for not running pipeline!");
		return true;
	}

	GstStateChangeReturn set_state_code = gst_element_set_state(pipeline_data.pipeline, GST_STATE_PAUSED);

	if (set_state_code == GST_STATE_CHANGE_FAILURE)
	{
		logger()->error("Failed to set the pipeline to the playing state!\n");
		return false;
	}

	logger()->info("Pipeline successfully set to PAUSED state with result code: %d", set_state_code);
}

bool Pipeline::is_pipeline_running() const
{
	if (!pipeline_data.pipeline)
	{
		return false;
	}

	GstState state;
	GstStateChangeReturn state_ret = gst_element_get_state(pipeline_data.pipeline, &state, NULL, 3 * GST_SECOND);
	if (state_ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
	{
		logger()->error("is_pipeline_running() could not retrieve pipeline state!");
		return false;
	}
	return state == GstState::GST_STATE_PLAYING;
}
