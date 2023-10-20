#pragma once

#include <gst/gst.h>
#include <QWidget>

class Pipeline
{
private:
	GstElement* gst_pipeline = nullptr;
	GstBus* gst_bus = nullptr;

private:
	void handle_pipeline_message(GstMessage* msg);

public:
	Pipeline(int port, WId windowHandle);
	~Pipeline();
	Pipeline& operator=(const Pipeline&) = delete;
	Pipeline(const Pipeline& copy) = delete;
	Pipeline() = delete;

	void setPort(int port);
	bool startPipeline();
	bool stopPipeline();
	bool isPipelinePlaying() const;
	void busPoll();
};