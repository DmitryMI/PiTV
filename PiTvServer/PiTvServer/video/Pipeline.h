#include <string>
#include <gst/gst.h>
#include <spdlog/spdlog.h>

struct PipelineConfig
{
	std::shared_ptr<spdlog::logger> logger_ptr;
	std::string recording_path = "Recordings";
	bool force_mkdirs = true;
	std::string camera_dev;
	int video_width = 640;
	int video_height = 640;
	int video_fps_numerator = 20;
	int video_fps_denominator = 1;
	int recording_segment_duration = 3600;
	int recording_max_size = 32*1024;
};

struct PipelineData
{
	PipelineConfig config;
	GstElement* pipeline = nullptr;
	GstBus* bus = nullptr;
	bool is_playing = false;
};

class Pipeline
{
private:
	PipelineData pipeline_data;

	std::shared_ptr<spdlog::logger> logger() const;

	GstElement* make_capturing_subpipe();
	GstElement* make_recording_subpipe();

	static const std::string get_current_date_time_str();
	void handle_pipeline_message(GstMessage* msg);
	static gchararray format_location_handler(GstElement* splitmux, guint fragment_id, gpointer udata);

public:
	Pipeline(const PipelineConfig& config);
	~Pipeline();
	Pipeline& operator=(const Pipeline&) = delete;
	Pipeline(const Pipeline& copy) = delete;
	Pipeline() = default;
	
	bool construct_pipeline();
	bool start_pipeline();
	bool pause_pipeline();
	bool is_pipeline_running() const;

	void bus_poll(int timeout_msec);
};