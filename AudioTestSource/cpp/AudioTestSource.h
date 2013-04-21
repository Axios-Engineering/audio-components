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
#ifndef AUDIOTESTSOURCE_IMPL_H
#define AUDIOTESTSOURCE_IMPL_H
#include <gst/gst.h>

#include "AudioTestSource_base.h"

class AudioTestSource_i;

class AudioTestSource_i : public AudioTestSource_base
{
    ENABLE_LOGGING
    public: 
        AudioTestSource_i(const char *uuid, const char *label);
        ~AudioTestSource_i();
        int serviceFunction();

        void initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException);
	    void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);

	    void start() throw (CF::Resource::StartError, CORBA::SystemException);
	    void stop() throw (CF::Resource::StopError, CORBA::SystemException);

	    void configure (const CF::Properties& configProperties)
	      throw (CF::PropertySet::PartialConfiguration, CF::PropertySet::InvalidConfiguration, CORBA::SystemException);

    protected:
	    void validate (CF::Properties property, CF::Properties& validProps, CF::Properties& invalidProps);

    private:
	    BULKIO::StreamSRI sri;
	    bool sri_changed;

		GstElement *pipeline;

		GstElement *src;
		GstElement *conv;
		GstElement *resamp;
		GstElement *sink;

	    void _set_gst_src_param(const std::string& propid);
	    void _set_gst_resamp_param(const std::string& propid);

	    static inline BULKIO::PrecisionUTCTime _now();
	    static inline BULKIO::PrecisionUTCTime _from_gst_timestamp(GstClockTime timestamp);

	    static gboolean _bus_callback(GstBus *bus, GstMessage *message, AudioTestSource_i* comp);
	    static void _new_gst_buffer(GstElement *sink, AudioTestSource_i *comp);
};

#endif
