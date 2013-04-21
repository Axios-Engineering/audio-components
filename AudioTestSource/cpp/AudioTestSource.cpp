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
#include <strstream>
#include "AudioTestSource.h"

PREPARE_LOGGING(AudioTestSource_i)

AudioTestSource_i::AudioTestSource_i(const char *uuid, const char *label) : 
    AudioTestSource_base(uuid, label)
{
	// Initialize and determine which version of GStreamer is being used.
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
	LOG_DEBUG(AudioTestSource_i, "Using GStreamer " << major << "." << minor << "." << micro << " " << nano_str);
}

AudioTestSource_i::~AudioTestSource_i()
{
}

void AudioTestSource_i::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException)
{
	AudioTestSource_base::initialize();

	// Tie SCA properties to GStreamer properties
	setPropertyChangeListener(static_cast<std::string>("waveform"), this, &AudioTestSource_i::_set_gst_src_param);
	setPropertyChangeListener(static_cast<std::string>("frequency"), this, &AudioTestSource_i::_set_gst_src_param);
	setPropertyChangeListener(static_cast<std::string>("volume"), this, &AudioTestSource_i::_set_gst_src_param);
	setPropertyChangeListener(static_cast<std::string>("is-live"), this, &AudioTestSource_i::_set_gst_src_param);
	setPropertyChangeListener(static_cast<std::string>("samplesperpacket"), this, &AudioTestSource_i::_set_gst_src_param);

	setPropertyChangeListener(static_cast<std::string>("resample-filter-length"), this, &AudioTestSource_i::_set_gst_src_param);
	setPropertyChangeListener(static_cast<std::string>("resample-quality"), this, &AudioTestSource_i::_set_gst_src_param);
}


void AudioTestSource_i::start() throw (CF::Resource::StartError, CORBA::SystemException)
{
	AudioTestSource_base::start();

	LOG_DEBUG (AudioTestSource_i, "Initializing GStreamer Pipeline");
	pipeline = gst_pipeline_new ("audio-pipeline");

	GstBus* bus = gst_pipeline_get_bus(reinterpret_cast<GstPipeline*>(pipeline));
	gst_bus_add_watch(bus, (GstBusFunc)AudioTestSource_i::_bus_callback, this);
	gst_object_unref (bus);

	src      = gst_element_factory_make ("audiotestsrc",  "audio");
	resamp   = gst_element_factory_make ("audioresample",  "resampler");
	conv     = gst_element_factory_make ("audioconvert",  "input_converter");
	sink     = gst_element_factory_make ("appsink",  "bio_out");

	std::strstream audio_type;
	audio_type << "audio/x-raw-int"
	           << ",channels=1"
	           << ",rate=" << sample_rate
			   << ",signed=(boolean)true"
			   << ",width=16"
			   << ",depth=16"
			   << ",endianness=1234";

	GstCaps *audio_caps = gst_caps_from_string (audio_type.str());
	g_object_set (sink, "caps", audio_caps, NULL);
	gst_caps_unref (audio_caps);

	g_object_set (sink, "emit-signals", TRUE, NULL);
	g_signal_connect (sink, "new-buffer", G_CALLBACK (AudioTestSource_i::_new_gst_buffer), this);

	_set_gst_src_param("*");
	_set_gst_resamp_param("*");

	gst_bin_add_many (GST_BIN (pipeline), src, resamp, conv, sink, NULL);

	if (!gst_element_link_many (src, resamp, conv, sink, NULL)) {
		LOG_WARN (AudioTestSource_i, "Failed to link elements!");
	}

	sri = BULKIO::StreamSRI();
	sri.hversion = 1;
	sri.xstart = 0.0;
	sri.xdelta = 1.0/sample_rate;
	sri.xunits = BULKIO::UNITS_TIME;
	sri.subsize = 0;
	sri.ystart = 0.0;
	sri.ydelta = 0.0;
	sri.yunits = BULKIO::UNITS_NONE;
	sri.mode = 0;
	sri.blocking = !is_live;
	sri.streamID = this->stream_id.c_str();

	sri.keywords.length(5);

	// New-style keyword for audio streams
	sri.keywords[0].id = CORBA::string_dup("AUDIO_TYPE");
	sri.keywords[0].value <<= audio_type;

	// Backwards compatibility
	sri.keywords[1].id = CORBA::string_dup("AUDIO_ENCODING");
	sri.keywords[1].value <<= "PCM_SIGNED";

	sri.keywords[2].id = CORBA::string_dup("AUDIO_CHANNELS");
	sri.keywords[2].value <<= 1;

	sri.keywords[3].id = CORBA::string_dup("AUDIO_FRAME_SIZE");
	sri.keywords[3].value <<= 2;

	sri.keywords[4].id = CORBA::string_dup("AUDIO_FRAME_RATE");
	sri.keywords[4].value <<= static_cast<float>(sample_rate);

	audio_out->pushSRI(sri);

	LOG_DEBUG (AudioTestSource_i, "Starting GStreamer Pipeline");
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

void AudioTestSource_i::stop() throw (CF::Resource::StopError, CORBA::SystemException)
{
	LOG_DEBUG (AudioTestSource_i, "Stopping GStreamer Pipeline");
	gst_element_set_state (pipeline, GST_STATE_NULL);

	LOG_DEBUG (AudioTestSource_i, "Releasing GStreamer Pipeline");
	gst_object_unref (GST_OBJECT (pipeline));

	pipeline = NULL;
	src      = NULL;
	resamp   = NULL;
	conv     = NULL;
	sink     = NULL;

	AudioTestSource_base::stop();
}

void AudioTestSource_i::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
	AudioTestSource_base::releaseObject();
}

gboolean AudioTestSource_i::_bus_callback(GstBus *bus, GstMessage *message, AudioTestSource_i* comp)
{
    switch(GST_MESSAGE_TYPE(message)){

    case GST_MESSAGE_ERROR:{
        gchar *debug;
        GError *err;

        gst_message_parse_error(message, &err, &debug);
        LOG_ERROR(AudioTestSource_i, "Gstreamer Error: " << err->message);
        g_error_free(err);
        g_free(debug);
    }
    break;

    case GST_MESSAGE_EOS:
    	LOG_INFO(AudioTestSource_i, "End of stream");
        break;

    default:
    	LOG_INFO(AudioTestSource_i, "Received gstreamer message: " <<gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
        break;
    }

    return TRUE;
}

void AudioTestSource_i::_new_gst_buffer(GstElement *sink, AudioTestSource_i* comp) {
	static GstBuffer *buffer;
	static std::vector<short> packet;

    /* Retrieve the buffer */
    g_signal_emit_by_name (sink, "pull-buffer", &buffer);
    if (buffer) {
    	BULKIO::PrecisionUTCTime T;

	    /* The only thing we do in this example is print a * to indicate a received buffer */
    	if (GST_CLOCK_TIME_IS_VALID(buffer->timestamp)) {
    		T = _from_gst_timestamp(buffer->timestamp);
    	} else {
    		T = _now();
    	}

    	packet.resize(buffer->size / 2); // TODO the division should come from reading buffer->caps
    	memcpy(&packet[0], buffer->data, buffer->size);

    	comp->audio_out->pushPacket(packet, T, false, comp->stream_id);
	    gst_buffer_unref (buffer);
    }
}

void AudioTestSource_i::validate(CF::Properties property, CF::Properties& validProps, CF::Properties& invalidProps)
{
    for (CORBA::ULong ii = 0; ii < property.length (); ++ii) {
        std::string id((const char*)property[ii].id);
        // Certian properties cannot be set while the component is running
        if (_started) {
            if (id == "sample-rate") {
            	LOG_WARN(AudioTestSource_i, "'sample-rate' cannot be changed while component is running.")
                CORBA::ULong count = invalidProps.length();
                invalidProps.length(count + 1);
                invalidProps[count].id = property[ii].id;
                invalidProps[count].value = property[ii].value;
            } else if (id == "is-live") {
            	LOG_WARN(AudioTestSource_i, "'is-live' cannot be changed while component is running.")
            	CORBA::ULong count = invalidProps.length();
				invalidProps.length(count + 1);
				invalidProps[count].id = property[ii].id;
				invalidProps[count].value = property[ii].value;
            } else if (id == "stream_id") {
            	LOG_WARN(AudioTestSource_i, "'is-stream_id' cannot be changed while component is running.")
            	CORBA::ULong count = invalidProps.length();
				invalidProps.length(count + 1);
				invalidProps[count].id = property[ii].id;
				invalidProps[count].value = property[ii].value;
            }
        }
    }

}

void AudioTestSource_i::configure (const CF::Properties& configProperties)
    throw (CF::PropertySet::PartialConfiguration,
           CF::PropertySet::InvalidConfiguration,
           CORBA::SystemException)
{
    CF::Properties validProperties;
    CF::Properties invalidProperties;
    validate(configProperties, validProperties, invalidProperties);

    if (invalidProperties.length() > 0) {
        throw CF::PropertySet::InvalidConfiguration("Properties failed validation.  See log for details.", invalidProperties);
    }

    PropertySet_impl::configure(configProperties);
}

void AudioTestSource_i::_set_gst_src_param(const std::string& propid) {
	if (src == NULL) return;

    LOG_DEBUG (AudioTestSource_i, "Changed source param " << propid)
    if ((propid == "waveform") || (propid == "*")) {
	    g_object_set (G_OBJECT (src), "wave", waveform, NULL);
    }
    if ((propid == "frequency") || (propid == "*")) {
	    g_object_set (G_OBJECT (src), "freq", (gdouble) frequency, NULL);
    }
    if ((propid == "volume") || (propid == "*")) {
	    g_object_set (G_OBJECT (src), "volume", volume, NULL);
    }
    if ((propid == "is-live") || (propid == "*")) {
	    g_object_set (G_OBJECT (src), "is-live", is_live, NULL);
    }
    if ((propid == "samplesperpacket") || (propid == "*")) {
	    g_object_set (G_OBJECT (src), "samplesperbuffer", samplesperpacket, NULL);
    }
}

void AudioTestSource_i::_set_gst_resamp_param(const std::string& propid) {
	if (resamp == NULL) return;

    LOG_DEBUG (AudioTestSource_i, "Changed resampler param " << propid)
    if ((propid == "resample-filter-length") || (propid == "*")) {
	    g_object_set (G_OBJECT (resamp), "filter-length", resample_filter_length, NULL);
    }
    if ((propid == "resample-quality") || (propid == "*")) {
	    g_object_set (G_OBJECT (resamp), "quality", resample_quality, NULL);
    }
}

inline BULKIO::PrecisionUTCTime AudioTestSource_i::_now() {
	struct timeval tmp_time;
	struct timezone tmp_tz;
	gettimeofday(&tmp_time, &tmp_tz);
	double wsec = tmp_time.tv_sec;
	double fsec = tmp_time.tv_usec / 1e6;;

	BULKIO::PrecisionUTCTime tstamp = BULKIO::PrecisionUTCTime();
	tstamp.tcmode = BULKIO::TCM_CPU;
	tstamp.tcstatus = (short)1;
	tstamp.toff = 0.0;
	tstamp.twsec = wsec;
	tstamp.tfsec = fsec;

	return tstamp;
}

inline BULKIO::PrecisionUTCTime AudioTestSource_i::_from_gst_timestamp(GstClockTime timestamp) {
	GTimeVal tv;
	GST_TIME_TO_TIMEVAL(timestamp, tv);

	BULKIO::PrecisionUTCTime tstamp = BULKIO::PrecisionUTCTime();
	tstamp.tcmode = BULKIO::TCM_CPU;
	tstamp.tcstatus = (short)1;
	tstamp.toff = 0.0;
	tstamp.twsec = tv.tv_sec;
	tstamp.tfsec = tv.tv_usec * 1.0e-6;

	return tstamp;
}

int AudioTestSource_i::serviceFunction()
{
    // Nothing to do currently
    return NOOP;
}
