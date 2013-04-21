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
#include "AudioSource.h"

PREPARE_LOGGING(AudioSource_i)

AudioSource_i::AudioSource_i(const char *uuid, const char *label) : 
    AudioSource_base(uuid, label)
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
	LOG_DEBUG(AudioSource_i, "Using GStreamer " << major << "." << minor << "." << micro << " " << nano_str);
}

AudioSource_i::~AudioSource_i()
{
}

void AudioSource_i::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException)
{
	AudioSource_base::initialize();
}


void AudioSource_i::start() throw (CF::Resource::StartError, CORBA::SystemException)
{
	LOG_DEBUG (AudioSource_i, "Initializing GStreamer Pipeline");
	pipeline = gst_element_factory_make ("playbin",  "audio");
	bus      = gst_pipeline_get_bus(reinterpret_cast<GstPipeline*>(pipeline));
	sink     = gst_element_factory_make ("appsink",  "bio_out");
	vsink    = gst_element_factory_make ("fakesink",  "video_out");

	_set_gst_vol_param("*");

	std::strstream audio_type;
	audio_type << "audio/x-raw-int"
	           << ",channels=1"
	           << ",rate=" << output_sample_rate
			   << ",signed=(boolean)true"
			   << ",width=16"
			   << ",depth=16"
			   << ",endianness=1234";

	LOG_INFO(AudioSource_i, "Playing audio source " << audio_uri);
	g_object_set (pipeline, "uri", audio_uri.c_str(), NULL);
	g_object_set (pipeline, "audio-sink", sink, "video-sink", vsink, NULL);
	//g_object_set (pipeline, "flags", GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_SOFT_VOLUME, NULL);

	GstCaps *audio_caps = gst_caps_from_string (audio_type.str());
	g_object_set (sink, "caps", audio_caps, NULL);
	gst_caps_unref (audio_caps);

	g_object_set (sink, "emit-signals", TRUE, NULL);
	g_signal_connect (sink, "new-buffer", G_CALLBACK (AudioSource_i::_new_gst_buffer), this);

	sri = BULKIO::StreamSRI();
	sri.hversion = 1;
	sri.xstart = 0.0;
	sri.xdelta = 1.0/output_sample_rate;
	sri.xunits = BULKIO::UNITS_TIME;
	sri.subsize = 0;
	sri.ystart = 0.0;
	sri.ydelta = 0.0;
	sri.yunits = BULKIO::UNITS_NONE;
	sri.mode = 0;
	sri.blocking = true;
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
	sri.keywords[4].value <<= static_cast<float>(output_sample_rate);

	audio_out->pushSRI(sri);

	LOG_DEBUG (AudioSource_i, "Starting GStreamer Pipeline");
	gst_element_set_state (pipeline, GST_STATE_PLAYING);

	AudioSource_base::start();
}

void AudioSource_i::stop() throw (CF::Resource::StopError, CORBA::SystemException)
{
	LOG_DEBUG (AudioSource_i, "Stopping GStreamer Pipeline");
	gst_element_set_state (pipeline, GST_STATE_NULL);

	LOG_DEBUG (AudioSource_i, "Releasing GStreamer Pipeline");
	gst_object_unref (GST_OBJECT (pipeline));

	gst_object_unref (bus);

	pipeline = NULL;
	sink     = NULL;

	AudioSource_base::stop();
}

void AudioSource_i::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
	AudioSource_base::releaseObject();
}

void AudioSource_i::_new_gst_buffer(GstElement *sink, AudioSource_i* comp) {
	static GstBuffer *buffer;
	static std::vector<short> packet;

    /* Retrieve the buffer */
    g_signal_emit_by_name (sink, "pull-buffer", &buffer);
    if (buffer) {
    	BULKIO::PrecisionUTCTime T;

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

void AudioSource_i::validate(CF::Properties property, CF::Properties& validProps, CF::Properties& invalidProps)
{
    for (CORBA::ULong ii = 0; ii < property.length (); ++ii) {
        std::string id((const char*)property[ii].id);
        // Certian properties cannot be set while the component is running
        if (_started) {
            if (id == "audio-uri") {
            	LOG_WARN(AudioSource_i, "'audio-uri' cannot be changed while component is running.")
                CORBA::ULong count = invalidProps.length();
                invalidProps.length(count + 1);
                invalidProps[count].id = property[ii].id;
                invalidProps[count].value = property[ii].value;
            } else if (id == "output-sample-rate") {
            	LOG_WARN(AudioSource_i, "'output-sample-rate' cannot be changed while component is running.")
                CORBA::ULong count = invalidProps.length();
                invalidProps.length(count + 1);
                invalidProps[count].id = property[ii].id;
                invalidProps[count].value = property[ii].value;
            }
        }
    }
}

void AudioSource_i::configure (const CF::Properties& configProperties)
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

inline BULKIO::PrecisionUTCTime AudioSource_i::_now() {
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

inline BULKIO::PrecisionUTCTime AudioSource_i::_from_gst_timestamp(GstClockTime timestamp)
{
	GTimeVal tv;
	GST_TIME_TO_TIMEVAL(timestamp, tv);

	BULKIO::PrecisionUTCTime tstamp = BULKIO::PrecisionUTCTime();
	tstamp.tcmode = BULKIO::TCM_OFF;
	tstamp.tcstatus = (short)1;
	tstamp.toff = 0.0;
	tstamp.twsec = tv.tv_sec;
	tstamp.tfsec = tv.tv_usec * 1.0e-6;

	return tstamp;
}

void AudioSource_i::loop()
{
	if (loop_count != 0) { // != 0 is on purpose, negative means loop forever
		if (!gst_element_seek(pipeline,
					1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
					GST_SEEK_TYPE_SET, 0,
					GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
			LOG_ERROR(AudioSource_i, "Failed to loop");
		}
		gst_element_set_state (pipeline, GST_STATE_PLAYING);
		if (loop_count > 0) --loop_count;
		LOG_INFO(AudioSource_i, "Loops remaining: " << loop_count);
	}
}

int AudioSource_i::serviceFunction()
{
    GstMessage* message = gst_bus_timed_pop_filtered(bus, GST_MSECOND, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    if (message == 0) {
    	return NOOP;
    }

    switch(GST_MESSAGE_TYPE(message)){

       case GST_MESSAGE_ERROR:{
           gchar *debug;
           GError *err;

           gst_message_parse_error(message, &err, &debug);
           LOG_ERROR(AudioSource_i, "Gstreamer Error: " << err->message);
           g_error_free(err);
           g_free(debug);
       }
       break;

       case GST_MESSAGE_EOS:
       	   LOG_DEBUG(AudioSource_i, "End of stream");
       	   loop();
           break;

       default:
       	LOG_INFO(AudioSource_i, "Received gstreamer message: " <<gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
           break;
       }

    return NORMAL;
}

void AudioSource_i::_set_gst_vol_param(const std::string& propid) {
	if (pipeline == NULL) return;

    LOG_DEBUG (AudioSource_i, "Changed vol param " << propid)
    if ((propid == "mute") || (propid == "*")) {
	    g_object_set (G_OBJECT (pipeline), "mute", mute, NULL);
    }
    if ((propid == "volume") || (propid == "*")) {
	    g_object_set (G_OBJECT (pipeline), "volume", (gdouble) volume, NULL);
    }
}
