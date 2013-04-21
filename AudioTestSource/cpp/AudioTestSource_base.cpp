
#include "AudioTestSource_base.h"

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY
    
 	Source: AudioTestSource.spd.xml
 	Generated on: Sun Apr 21 01:22:50 BST 2013
 	REDHAWK IDE
 	Version: R.1.8.3
 	Build id: v201303122306

*******************************************************************************************/

/******************************************************************************************

    The following class functions are for the base class for the component class. To
    customize any of these functions, do not modify them here. Instead, overload them
    on the child class

******************************************************************************************/
 
AudioTestSource_base::AudioTestSource_base(const char *uuid, const char *label) :
                                     Resource_impl(uuid, label), serviceThread(0) {
    construct();
}

void AudioTestSource_base::construct()
{
    Resource_impl::_started = false;
    loadProperties();
    serviceThread = 0;
    
    PortableServer::ObjectId_var oid;
    audio_out = new BULKIO_dataShort_Out_i("audio_out", this);
    oid = ossie::corba::RootPOA()->activate_object(audio_out);

    registerOutPort(audio_out, audio_out->_this());
}

/*******************************************************************************************
    Framework-level functions
    These functions are generally called by the framework to perform housekeeping.
*******************************************************************************************/
void AudioTestSource_base::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException)
{
}

void AudioTestSource_base::start() throw (CORBA::SystemException, CF::Resource::StartError)
{
    boost::mutex::scoped_lock lock(serviceThreadLock);
    if (serviceThread == 0) {
        serviceThread = new ProcessThread<AudioTestSource_base>(this, 0.1);
        serviceThread->start();
    }
    
    if (!Resource_impl::started()) {
    	Resource_impl::start();
    }
}

void AudioTestSource_base::stop() throw (CORBA::SystemException, CF::Resource::StopError)
{
    boost::mutex::scoped_lock lock(serviceThreadLock);
    // release the child thread (if it exists)
    if (serviceThread != 0) {
        if (!serviceThread->release(2)) {
            throw CF::Resource::StopError(CF::CF_NOTSET, "Processing thread did not die");
        }
        serviceThread = 0;
    }
    
    if (Resource_impl::started()) {
    	Resource_impl::stop();
    }
}

CORBA::Object_ptr AudioTestSource_base::getPort(const char* _id) throw (CORBA::SystemException, CF::PortSupplier::UnknownPort)
{

    std::map<std::string, Port_Provides_base_impl *>::iterator p_in = inPorts.find(std::string(_id));
    if (p_in != inPorts.end()) {

    }

    std::map<std::string, CF::Port_var>::iterator p_out = outPorts_var.find(std::string(_id));
    if (p_out != outPorts_var.end()) {
        return CF::Port::_duplicate(p_out->second);
    }

    throw (CF::PortSupplier::UnknownPort());
}

void AudioTestSource_base::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
    // This function clears the component running condition so main shuts down everything
    try {
        stop();
    } catch (CF::Resource::StopError& ex) {
        // TODO - this should probably be logged instead of ignored
    }

    // deactivate ports
    releaseInPorts();
    releaseOutPorts();

    delete(audio_out);
 
    Resource_impl::releaseObject();
}

void AudioTestSource_base::loadProperties()
{
    addProperty(waveform,
                0, 
               "waveform",
               "",
               "readwrite",
               "",
               "ge",
               "configure");

    addProperty(frequency,
                440, 
               "frequency",
               "",
               "readwrite",
               "Hz",
               "external",
               "configure");

    addProperty(is_live,
                true, 
               "is-live",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(samplesperpacket,
                1024, 
               "samplesperpacket",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(volume,
                0.8, 
               "volume",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(sample_rate,
                8000, 
               "sample-rate",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(resample_filter_length,
                64, 
               "resample-filter-length",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(resample_quality,
                4, 
               "resample-quality",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(stream_id,
               "stream_id",
               "",
               "readwrite",
               "",
               "external",
               "configure");

}
