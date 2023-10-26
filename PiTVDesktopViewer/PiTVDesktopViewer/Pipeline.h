#pragma once

#include <gst/gst.h>
#include <QWidget>

class Pipeline
{
private:
	GstElement* gst_pipeline = nullptr;
	GstBus* gst_bus = nullptr;
	int port;
	WId windowHandle;

	QByteArray srtpKey = QByteArray(46, 0);
	int srtpCipher = 0;
	int srtpAuth = 0;

private:
	void handle_pipeline_message(GstMessage* msg);

	static GstCaps* onSrtpRequestKey(GstElement* gstsrtpdec,guint ssrc,	gpointer udata);

public:
	Pipeline(int port, WId windowHandle);
	~Pipeline();
	Pipeline& operator=(const Pipeline&) = delete;
	Pipeline(const Pipeline& copy) = delete;
	Pipeline() = delete;

	bool constructPipeline();

	void setPort(int port);
	bool startPipeline();
	bool stopPipeline();
	bool isPipelinePlaying() const;
	void busPoll();

	bool srtpSetKey(QByteArray key);
	bool srtpSetSecurityParams(int cipher, int auth);
};