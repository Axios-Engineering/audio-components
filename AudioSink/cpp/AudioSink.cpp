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
    AudioSink_base(uuid, label),
    pipeline(0),
    bus(0),
    src(0),
    rate(0),
    conv(0),
    eqlzr(0),
    vol(0),
    resamp(0),
    queue(0),
    sink(0)
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
	pipeline = 0;
	bus = 0;
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

int AudioSink_i::serviceFunction()
{
    if (bus) {
        // Check the GST message queue for any possible messages of interest
        GstMessage* message = gst_bus_timed_pop_filtered(bus, 0, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        if (message != 0) {
            switch(GST_MESSAGE_TYPE(message)){

            case GST_MESSAGE_ERROR:{
                gchar *debug;
                GError *err;

                gst_message_parse_error(message, &err, &debug);
                LOG_ERROR(AudioSink_i, "Gstreamer Error: " << err->message);
                g_error_free(err);
                g_free(debug);
            }
            break;

            case GST_MESSAGE_EOS:
                LOG_DEBUG(AudioSink_i, "End of stream");
                break;

            default:
                LOG_INFO(AudioSink_i, "Received gstreamer message: " <<gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
                break;
            }
        }
    }

	// See if any data has arrived
	BULKIO_dataShort_In_i::dataTransfer *tmp = audio_in->getPacket(-1);
	if (not tmp) { // No data is available
		return NOOP;
	}

	// If the SRI xdelta has changed, tear-down and restart the pipeline
	// with the new sample rate.
	if ((tmp->sriChanged) && (current_sri.xdelta != tmp->SRI.xdelta)) {
		current_sri = tmp->SRI;
		_create_pipeline();
	}

	// If there is a src available and it wants data then feed it.
	if (src != 0) {
		GstBuffer *buffer = gst_buffer_new ();
		GST_BUFFER_SIZE (buffer) = tmp->dataBuffer.size()*2;
		GST_BUFFER_MALLOCDATA (buffer) = reinterpret_cast<guint8*>(g_malloc (GST_BUFFER_SIZE(buffer)));
		GST_BUFFER_DATA (buffer) = GST_BUFFER_MALLOCDATA (buffer);
		memcpy(GST_BUFFER_DATA (buffer), &tmp->dataBuffer[0], GST_BUFFER_SIZE (buffer));

		if ((tmp->T.tcstatus == BULKIO::TCS_VALID) && (IGNORE_TIMESTAMPS == false)) {
			GTimeVal tv;
			tv.tv_sec = tmp->T.twsec;
			tv.tv_usec = (tmp->T.tfsec * 1.0e6);
			GST_BUFFER_TIMESTAMP (buffer) = GST_TIMEVAL_TO_TIME(tv);
		}

		int sample_rate = static_cast<int>(rint(1.0/current_sri.xdelta));
  	    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (tmp->dataBuffer.size(), GST_SECOND, sample_rate); // number of nanoseconds in buffer val*num/denom

		gst_app_src_push_buffer(reinterpret_cast<GstAppSrc*>(src), buffer);
	}
	delete tmp;
    
    return NORMAL;
}

void AudioSink_i::_create_pipeline() {
	// Tear-down the previous pipeline
	if (pipeline != 0) {
		LOG_DEBUG (AudioSink_i, "Stopping GStreamer Pipeline");
		gst_element_set_state (pipeline, GST_STATE_NULL);

		LOG_DEBUG (AudioSink_i, "Releasing GStreamer Pipeline");
		gst_object_unref (GST_OBJECT (pipeline));

		pipeline = 0;
		src = 0;
		rate = 0;
		conv = 0;
		eqlzr = 0;
		vol = 0;
		resamp = 0;
		sink = 0;
	}

	// If the current SRI lacks a sample rate refuse playback
	if (current_sri.xdelta == 0) {
		LOG_WARN(AudioSink_i, "Cannot playback BIO stream with 0 second xdelta");
		return;
	}
	int sample_rate = static_cast<int>(rint(1.0/current_sri.xdelta));

	LOG_DEBUG (AudioSink_i, "Initializing GStreamer Pipeline");
	pipeline = gst_pipeline_new ("audio-pipeline");
	bus = gst_pipeline_get_bus(reinterpret_cast<GstPipeline*>(pipeline));

	// Source from the BULKIO port
	src      = gst_element_factory_make ("appsrc",  "bio_in");
	std::strstream audio_type;
	audio_type << "audio/x-raw-int"
			   << ",channels=1"
			   << ",rate=" << sample_rate
			   << ",signed=(boolean)true"
			   << ",width=16"
			   << ",depth=16"
			   << ",endianness=1234";
	LOG_DEBUG (AudioSink_i, "Rendering audio type: " << audio_type.str());
	GstCaps *audio_caps = gst_caps_from_string (audio_type.str());
	g_object_set (src, "caps", audio_caps, NULL);
	g_object_set(src, "block", true, (const void*) 0);
	gst_caps_unref (audio_caps);

	// Convert the audio format if necessary
	conv     = gst_element_factory_make ("audioconvert",  "converter");

	// Use audiorate to create a "perfect" stream from a potentially imperfect source
	rate     = gst_element_factory_make ("audiorate",  "rate");

	// Add an equalizer and volume control
	eqlzr    = gst_element_factory_make ("equalizer-3bands", "eq");
	vol      = gst_element_factory_make ("volume", "vol");
	_set_gst_eqlzr_param("*");
	_set_gst_vol_param("*");

	// Resample if necessary to connect to the audio output sink
	resamp   = gst_element_factory_make ("audioresample",  "resampler");

    // Provide a queue so that the audio playback can tolerate some
    // jitter
    queue = gst_element_factory_make("queue", "queue");
    g_object_set(queue, "min-threshold-time", 2500 * GST_MSECOND,
            "max-size-time", 5000 * GST_MSECOND, (const void*) 0);
    // Attach callbacks to pause the pipe if there is a queue underrun
    g_signal_connect(queue, "underrun", G_CALLBACK(AudioSink_i::_underrun), this);
    g_signal_connect(queue, "pushing", G_CALLBACK(AudioSink_i::_pushing), this);

	// Output to the best-guess audio output
	sink     = gst_element_factory_make ("autoaudiosink", "soundcard");

	gst_bin_add_many (GST_BIN (pipeline), src, conv, queue, rate, eqlzr, vol, resamp, sink, NULL);

	if (!gst_element_link_many (src, conv, queue, rate, eqlzr, vol, resamp, sink, NULL)) {
		LOG_WARN (AudioSink_i, "Failed to link elements!");
	}

	LOG_DEBUG (AudioSink_i, "Starting GStreamer Pipeline");
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
}


void AudioSink_i::_pushing(GstElement *sink, AudioSink_i* comp)
{
    LOG_DEBUG(AudioSink_i, "Audio Running");
    if (comp->pipeline) {
        gst_element_set_state(comp->pipeline, GST_STATE_PLAYING);
    }
}

void AudioSink_i::_underrun(GstElement *sink, AudioSink_i* comp)
{
    LOG_DEBUG(AudioSink_i, "Audio Underrun");
    if (comp->pipeline) {
        gst_element_set_state(comp->pipeline, GST_STATE_PAUSED);
    }
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
