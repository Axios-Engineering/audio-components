/**
 * Copyright (C) 2013 Axios, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <gst/app/gstappsrc.h>
#include <strstream>

#include "AudioSink.h"

PREPARE_LOGGING(AudioSink_i)

AudioSink_i::AudioSink_i(const char *uuid, const char *label) : 
    AudioSink_base(uuid, label)
{
	const gchar *nano_str;
	guint major, minor, micro, nano;

	gst_init(0, 0);

	gst_version (&major, &minor, &micro, &nano);
	if (nano == 1) {
		nano_str = "(CVS)";
	} else if (nano == 2) {
		nano_str = "(Prerelease)";
	} else {
		nano_str = "";
	}
	LOG_DEBUG(AudioSink_i, "Using GStreamer " << major << "." << minor << "." << micro << " " << nano_str);
}

AudioSink_i::~AudioSink_i()
{
}

void AudioSink_i::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException)
{
	LOG_DEBUG (AudioSink_i, "Initializing");

	// Tie SCA properties to GStreamer properties
	setPropertyChangeListener(static_cast<std::string>("equalizer"), this, &AudioSink_i::_set_gst_eqlzr_param);
	setPropertyChangeListener(static_cast<std::string>("volume"), this, &AudioSink_i::_set_gst_vol_param);
	setPropertyChangeListener(static_cast<std::string>("mute"), this, &AudioSink_i::_set_gst_vol_param);
}

void AudioSink_i::start() throw (CF::Resource::StartError, CORBA::SystemException)
{
	if (pipeline) {
		LOG_DEBUG (AudioSink_i, "Starting GStreamer Pipeline");
		gst_element_set_state (pipeline, GST_STATE_PLAYING);
	}

	AudioSink_base::start();
}

void AudioSink_i::stop() throw (CF::Resource::StopError, CORBA::SystemException)
{
	if (pipeline) {
		LOG_DEBUG (AudioSink_i, "Stopping GStreamer Pipeline");
		gst_element_set_state (pipeline, GST_STATE_NULL);
	}

	AudioSink_base::stop();
}

void AudioSink_i::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
	AudioSink_base::releaseObject();

	if (pipeline) {
		LOG_DEBUG (AudioSink_i, "Releasing GStreamer Pipeline");
		gst_object_unref (GST_OBJECT (pipeline));
	}
}

void AudioSink_i::_start_feed(GstElement *sink, guint size, AudioSink_i* comp)
{
    comp->feed_gst = true;
}

void AudioSink_i::_stop_feed(GstElement *sink, guint size, AudioSink_i* comp)
{
	comp->feed_gst = false;
}

gboolean AudioSink_i::_bus_callback(GstBus *bus, GstMessage *message, AudioSink_i* comp)
{
    switch(GST_MESSAGE_TYPE(message)){

    case GST_MESSAGE_ERROR:{
        gchar *debug;
        GError *err;

        gst_message_parse_error(message, &err, &debug);
        g_print("Error %s\n", err->message);
        g_error_free(err);
        g_free(debug);
    }
    break;

    case GST_MESSAGE_EOS:
        g_print("End of stream\n");
        break;

    default:
        g_print("got message %s\n", \
            gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
        break;
    }

    return TRUE;
}

int AudioSink_i::serviceFunction()
{

	BULKIO_dataShort_In_i::dataTransfer *tmp = audio_in->getPacket(-1);
	if (not tmp) { // No data is available
		return NOOP;
	}

	if ((tmp->sriChanged) && (current_sri.xdelta != tmp->SRI.xdelta)) {
		current_sri = tmp->SRI;
		_create_pipeline();
	}

	if ((src != 0) && (feed_gst)) {
		GstBuffer *buffer = gst_buffer_new ();
		GST_BUFFER_SIZE (buffer) = tmp->dataBuffer.size()*2;
		GST_BUFFER_MALLOCDATA (buffer) = reinterpret_cast<guint8*>(g_malloc (GST_BUFFER_SIZE(buffer)));
		GST_BUFFER_DATA (buffer) = GST_BUFFER_MALLOCDATA (buffer);
		memcpy(GST_BUFFER_DATA (buffer), &tmp->dataBuffer[0], GST_BUFFER_SIZE (buffer));

		if (tmp->T.tcstatus == BULKIO::TCS_VALID) {
			GTimeVal tv;
			tv.tv_sec = tmp->T.twsec;
			tv.tv_usec = (tmp->T.tfsec * 1.0e6);
			GST_BUFFER_TIMESTAMP (buffer) = GST_TIMEVAL_TO_TIME(tv);
		}

		int sample_rate = static_cast<int>(rint(1.0/current_sri.xdelta));
  	    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (tmp->dataBuffer.size(), GST_SECOND, sample_rate); // number of nanoseconds in buffer val*num/denom
		gst_app_src_push_buffer(reinterpret_cast<GstAppSrc*>(src), buffer);
	}
    
    return NORMAL;
}

void AudioSink_i::_create_pipeline() {
	if (pipeline != 0) {
		LOG_DEBUG (AudioSink_i, "Stopping GStreamer Pipeline");
		gst_element_set_state (pipeline, GST_STATE_NULL);

		LOG_DEBUG (AudioSink_i, "Releasing GStreamer Pipeline");
		gst_object_unref (GST_OBJECT (pipeline));

		pipeline = 0;
		src = 0;
		conv = 0;
		resamp = 0;
		sink = 0;
	}

	if (current_sri.xdelta == 0) {
		LOG_WARN(AudioSink_i, "Cannot playback BIO stream with 0 second xdelta");
		return;
	}

	int sample_rate = static_cast<int>(rint(1.0/current_sri.xdelta));
	std::strstream audio_type;
	audio_type << "audio/x-raw-int"
			   << ",channels=1"
			   << ",rate=" << sample_rate
			   << ",signed=(boolean)true"
			   << ",width=16"
			   << ",depth=16"
			   << ",endianness=1234";

	LOG_DEBUG (AudioSink_i, "Initializing GStreamer Pipeline");
	pipeline = gst_pipeline_new ("audio-pipeline");

	GstBus* bus = gst_pipeline_get_bus(reinterpret_cast<GstPipeline*>(pipeline));
	gst_bus_add_watch(bus, (GstBusFunc)AudioSink_i::_bus_callback, this);

	src      = gst_element_factory_make ("appsrc",  "bio_in");
	conv     = gst_element_factory_make ("audioconvert",  "converter");
	eqlzr    = gst_element_factory_make ("equalizer-3bands", "eq");
	vol      = gst_element_factory_make ("volume", "vol");
	resamp   = gst_element_factory_make ("audioresample",  "resampler");
	sink     = gst_element_factory_make ("autoaudiosink", "soundcard");

	LOG_INFO (AudioSink_i, "Rendering audio type: " << audio_type.str());
	GstCaps *audio_caps = gst_caps_from_string (audio_type.str());
	g_object_set (src, "caps", audio_caps, NULL);
	gst_caps_unref (audio_caps);

	g_signal_connect (src, "need-data", G_CALLBACK (AudioSink_i::_start_feed), this);
	g_signal_connect (src, "enough-data", G_CALLBACK (AudioSink_i::_stop_feed), this);

	_set_gst_eqlzr_param("*");
	_set_gst_vol_param("*");

	gst_bin_add_many (GST_BIN (pipeline), src, conv, eqlzr, vol, resamp, sink, NULL);

	if (!gst_element_link_many (src, conv, eqlzr, vol, resamp, sink, NULL)) {
		LOG_WARN (AudioSink_i, "Failed to link elements!");
	}

	LOG_DEBUG (AudioSink_i, "Starting GStreamer Pipeline");
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

void AudioSink_i::_set_gst_eqlzr_param(const std::string& propid) {
	if (eqlzr == NULL) return;

    LOG_DEBUG (AudioSink_i, "Changed eqlzr param " << propid)
    if ((propid == "equalizer") || (propid == "*")) {
	    g_object_set (G_OBJECT (eqlzr), "band0", equalizer.low, NULL);
	    g_object_set (G_OBJECT (eqlzr), "band1", equalizer.med, NULL);
	    g_object_set (G_OBJECT (eqlzr), "band2", equalizer.hi, NULL);
    }
}

void AudioSink_i::_set_gst_vol_param(const std::string& propid) {
	if (vol == NULL) return;

    LOG_DEBUG (AudioSink_i, "Changed vol param " << propid)
    if ((propid == "mute") || (propid == "*")) {
	    g_object_set (G_OBJECT (vol), "mute", mute, NULL);
    }
    if ((propid == "volume") || (propid == "*")) {
	    g_object_set (G_OBJECT (vol), "volume", (gdouble) volume, NULL);
    }
}
