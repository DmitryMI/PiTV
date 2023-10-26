#pragma once

#include <string>
#include <gst/gst.h>
#include <spdlog/spdlog.h>
#include <filesystem>

struct PipelineConfig
{
	std::shared_ptr<spdlog::logger> logger_ptr;
	std::string recording_path = "Recordings";
	bool force_mkdirs = true;
	int video_width = 640;
	int video_height = 640;
	int video_fps_numerator = 20;
	int video_fps_denominator = 1;
	int recording_segment_duration = 3600;
	int recording_max_size = 32 * 1024;
	std::string videosource_override;
};

class Pipeline
{
public:
	static const std::string recording_extension;
	static const int srtp_key_length;
private:
	PipelineConfig config;
	GstElement* gst_pipeline = nullptr;
	GstBus* bus = nullptr;
	bool is_playing = false;
	std::string recording_full_path;

	std::mutex srtp_master_key_mutex;
	std::vector<uint8_t> srtp_master_key;

	static const std::string get_current_date_time_str();
	static gchararray format_location_handler(GstElement* splitmux, guint fragment_id, gpointer udata);
	static void on_srtp_soft_limit(GstElement* gstsrtpenc,	gpointer udata);

	std::shared_ptr<spdlog::logger> logger() const;

	GstElement* make_capturing_subpipe();
	GstElement* make_recording_subpipe();
	GstElement* make_streaming_subpipe();

	uintmax_t get_recording_total_size() const;
	std::filesystem::path get_oldest_file() const;
	void enforce_recording_max_size_restrictions(std::filesystem::path last_fragment_path, int last_fragment_index);
	void handle_pipeline_message(GstMessage* msg);

	bool generate_srtp_master_key();
	bool srtpenc_set_key(GstElement* gstsrtpenc);

	template<typename Callable>
	void traverse_bin_elements(GstBin* bin, int level, const Callable& callable) const
	{
		GstIterator* bin_iterator = gst_bin_iterate_elements(bin);

		GValue item = G_VALUE_INIT;
		gboolean done = FALSE;
		while (!done)
		{
			switch (gst_iterator_next(bin_iterator, &item))
			{
			case GST_ITERATOR_OK:
			{
				GstElement* element = GST_ELEMENT(g_value_get_object(&item));
				callable(element, level);
				if (GST_IS_BIN(element))
				{
					traverse_bin_elements(GST_BIN(element), level + 1, callable);
				}
				g_value_reset(&item);
			}
			break;
			case GST_ITERATOR_RESYNC:
				gst_iterator_resync(bin_iterator);
				break;
			case GST_ITERATOR_ERROR:
				done = TRUE;
				break;
			case GST_ITERATOR_DONE:
				done = TRUE;
				break;
			}
		}
		g_value_unset(&item);
		gst_iterator_free(bin_iterator);
	}

	template<typename Filter>
	GstElement* find_bin_child(GstBin* bin, const Filter& filter)
	{
		GstElement* result = nullptr;
		traverse_bin_elements(bin, 0, [&result, filter](GstElement* element, int level)
			{
				if (filter(element))
				{
					result = element;
				}
			}
		);

		return result;
	}

	bool attach_rtp_bin(GstElement* element);
	bool detach_rtp_bin(GstElement* bin);

public:
	Pipeline(const PipelineConfig& config);
	~Pipeline();
	Pipeline& operator=(const Pipeline&) = delete;
	Pipeline(const Pipeline& copy) = delete;
	Pipeline() = default;

	bool construct_pipeline();
	bool start_pipeline();
	bool pause_pipeline();
	bool stop_pipeline();
	bool is_pipeline_running() const;
	void bus_poll(int timeout_msec);
	void bus_poll();

	bool set_recording_full_path();
	std::string get_recording_full_path() const;

	bool rtp_add_endpoint(std::string host, int port);
	bool rtp_remove_endpoint(std::string host, int port);
	bool rtp_change_endpoint(std::string host_old, int port_old, std::string host, int port);

	void dump_pipeline_dot(std::string name) const;

	template<typename Callable>
	void traverse_pipeline_elements(const Callable& callable) const
	{
		if (!gst_pipeline)
		{
			logger()->error("Cannot traverse not constructed pipeline!");
			return;
		}

		callable(gst_pipeline, 0);

		traverse_bin_elements(GST_BIN(gst_pipeline), 0, callable);
	}

	bool splitmux_split_after();

	static void print_pipeline_elements_state(GstElement* element, int indent_level, std::stringstream* msg_builder);

	void log_pipeline_elements_state() const;

	void set_config(const PipelineConfig& config);

	bool get_srtp_master_key(std::vector<uint8_t>& key_vec);

	bool get_srtp_security_params(int& cipher_index, int& auth_index);
};