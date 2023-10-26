#include "Pipeline.h"
#include <QDebug>
#include <gst/video/videooverlay.h>

void Pipeline::handle_pipeline_message(GstMessage* msg)
{
	GError* err;
	gchar* debug_info;

	switch (GST_MESSAGE_TYPE(msg))
	{
	case GST_MESSAGE_INFO:
		gst_message_parse_info(msg, &err, &debug_info);
		qInfo() << QString("Info from %1: %2\n").arg( GST_OBJECT_NAME(msg->src), err->message);
		qInfo() << QString("Debugging information: %1\n").arg(debug_info ? debug_info : "none");
		g_clear_error(&err);
		g_free(debug_info);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(msg, &err, &debug_info);
		qWarning() << QString("Warning from %1: %2\n").arg(GST_OBJECT_NAME(msg->src), err->message);
		qWarning() << QString("Debugging information: %1\n").arg(debug_info ? debug_info : "none");
		g_clear_error(&err);
		g_free(debug_info);
		break;
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, &debug_info);
		qCritical() << QString("Error from %1: %2\n").arg(GST_OBJECT_NAME(msg->src), err->message);
		qCritical() << QString("Debugging information: %1\n").arg(debug_info ? debug_info : "none");
		g_clear_error(&err);
		g_free(debug_info);
		break;
	case GST_MESSAGE_EOS:
		qCritical() << ("\nEnd-Of-Stream reached!\n");
		break;
	case GST_MESSAGE_STATE_CHANGED:
	{
		GstState old_state, new_state, pending_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

		GstObject* sender = GST_MESSAGE_SRC(msg);

		qInfo() << "Element " << GST_ELEMENT_NAME(GST_ELEMENT(sender)) << " state changed from" << 
			gst_element_state_get_name(old_state) << " to " <<
			gst_element_state_get_name(new_state);
	}
	break;
	default:
		/* We should not reach here */
		// g_printerr("Unexpected message received.\n");
		break;
	}
}

Pipeline::Pipeline(int port, WId windowHandle)
{
	gst_debug_set_active(true);
	gst_debug_set_default_threshold(GST_LEVEL_WARNING);

	this->port = port;
	this->windowHandle = windowHandle;
}

Pipeline::~Pipeline()
{
	if (gst_pipeline)
	{
		GstState state = GstState::GST_STATE_NULL;
		GstStateChangeReturn state_ret = gst_element_get_state(gst_pipeline, &state, NULL, 0);
		if (state_ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
		{
			qWarning() << "[~Pipeline] Failed to get pipeline's state";
		}
		else if (state != GstState::GST_STATE_NULL)
		{
			qDebug() << "[~Pipeline] Setting pipeline to NULL state...";
			GstStateChangeReturn ret = gst_element_set_state(gst_pipeline, GstState::GST_STATE_NULL);
			if (ret != GstStateChangeReturn::GST_STATE_CHANGE_SUCCESS)
			{
				qCritical() << "[~Pipeline] Failed to set pipeline to NULL state!";
			}
			else
			{
				qCritical() << "[~Pipeline] Pipeline set to NULL state successfully!";
			}
		}

		gst_object_unref(gst_pipeline);
	}

	if (gst_bus)
	{
		gst_object_unref(gst_bus);
	}
}

bool Pipeline::constructPipeline()
{
	GError* error;

	//QString launchStr = QString("udpsrc name=udpsrc port=%1 ! application/x-rtp,clock-rate=90000,payload=96 ! rtph264depay ! avdec_h264 ! glimagesink name=glimagesink").arg(port);
	//gst_pipeline = gst_parse_launch(launchStr.toStdString().c_str(), NULL);
	gst_pipeline = gst_pipeline_new("main-pipeline");
	Q_ASSERT(gst_pipeline);

	GstElement* udpsrc = gst_element_factory_make("udpsrc", "udpsrc");
	Q_ASSERT(udpsrc);
	g_object_set(udpsrc, "port", port, NULL);
	GstElement* capsfilter = gst_element_factory_make("capsfilter", NULL);
	Q_ASSERT(capsfilter);

	GstCaps* caps = gst_caps_new_simple(
		"application/x-rtp",
		"clock-rate", G_TYPE_INT, 90000,
		"payload", G_TYPE_STRING, "video",
		NULL);

	g_object_set(capsfilter, "caps", caps, NULL);

	GstElement* srtpdec = gst_element_factory_make("srtpdec", "srtpdec");
	Q_ASSERT(srtpdec);

	GstElement* rtph264depay = gst_element_factory_make("rtph264depay", "rtph264depay");
	Q_ASSERT(rtph264depay);

	GstElement* avdec_h264 = gst_element_factory_make("avdec_h264", "avdec_h264");
	Q_ASSERT(avdec_h264);

	GstElement* glimagesink = gst_element_factory_make("glimagesink", "glimagesink");
	Q_ASSERT(glimagesink);

	gst_bin_add_many(GST_BIN(gst_pipeline), udpsrc, srtpdec, capsfilter, rtph264depay, avdec_h264, glimagesink, NULL);

	if (!gst_element_link_many(udpsrc, srtpdec, capsfilter, rtph264depay, avdec_h264, glimagesink, NULL))
	{
		return false;
	}
	
	gst_bus = gst_element_get_bus(gst_pipeline);
	Q_ASSERT(gst_bus);

	gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(glimagesink), windowHandle);
	return gst_pipeline;
}

void Pipeline::setPort(int port)
{
	Q_ASSERT(gst_pipeline);
	GstElement* udpsrc = gst_bin_get_by_name(GST_BIN(gst_pipeline), "udpsrc");
	Q_ASSERT(udpsrc);
	g_object_set(udpsrc, "port", port, NULL);
}

bool Pipeline::startPipeline()
{
	Q_ASSERT(gst_pipeline);

	GstStateChangeReturn set_state_code = gst_element_set_state(gst_pipeline, GST_STATE_PLAYING);

	if (set_state_code == GST_STATE_CHANGE_FAILURE)
	{
		qCritical() << "Failed to set the pipeline to the playing state!";
		return false;
	}

	qInfo() << "Pipeline successfully set to PLAYING state with result code:" << set_state_code;
	return true;
}

bool Pipeline::stopPipeline()
{
	Q_ASSERT(gst_pipeline);

	GstStateChangeReturn set_state_code = gst_element_set_state(gst_pipeline, GST_STATE_NULL);

	if (set_state_code == GST_STATE_CHANGE_FAILURE)
	{
		qCritical() << "Failed to set the pipeline to the NULL state!";
		return false;
	}

	qInfo() << "Pipeline successfully set to NULL state with result code: " << set_state_code;
	return true;
}

bool Pipeline::isPipelinePlaying() const
{
	Q_ASSERT(gst_pipeline);

	GstState current;
	GstState pending;
	if (gst_element_get_state(gst_pipeline, &current, &pending, 100) == GstStateChangeReturn::GST_STATE_CHANGE_FAILURE)
	{
		qCritical() << "Failed to get pipeline state!";
		return false;
	}

	return current == GST_STATE_PLAYING;
}

void Pipeline::busPoll()
{
	if (!gst_bus)
	{
		qCritical() << ("Cannot poll buss, it is nullptr!");
		return;
	}

	GstMessage* message = gst_bus_pop_filtered(gst_bus,
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
