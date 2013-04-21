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
#ifndef AUDIOSINK_IMPL_H
#define AUDIOSINK_IMPL_H
#include <gst/gst.h>

#include "AudioSink_base.h"

class AudioSink_i;

class AudioSink_i : public AudioSink_base
{
    ENABLE_LOGGING
    public: 
        AudioSink_i(const char *uuid, const char *label);
        ~AudioSink_i();
        int serviceFunction();

        void initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException);
        void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);

	    void start() throw (CF::Resource::StartError, CORBA::SystemException);
	    void stop() throw (CF::Resource::StopError, CORBA::SystemException);

    private:
	    bool feed_gst;
		GstElement *pipeline;
		GstBus* bus;

		GstElement *src;
		GstElement *rate;
		GstElement *conv;
		GstElement *eqlzr;
		GstElement *vol;
		GstElement *resamp;
		GstElement *sink;

		BULKIO::StreamSRI current_sri;

	    void _set_gst_vol_param(const std::string& propid);
	    void _set_gst_eqlzr_param(const std::string& propid);
		void _create_pipeline();

		static void _start_feed(GstElement *src, guint size, AudioSink_i *comp);
		static void _stop_feed(GstElement *src, guint size, AudioSink_i *comp);
};

#endif
