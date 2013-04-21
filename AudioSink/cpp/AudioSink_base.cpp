
#include "AudioSink_base.h"

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY
    
 	Source: AudioSink.spd.xml
 	Generated on: Sun Apr 21 15:44:26 BST 2013
 	REDHAWK IDE
 	Version: R.1.8.3
 	Build id: v201303122306

*******************************************************************************************/

/******************************************************************************************

    The following class functions are for the base class for the component class. To
    customize any of these functions, do not modify them here. Instead, overload them
    on the child class

******************************************************************************************/
 
AudioSink_base::AudioSink_base(const char *uuid, const char *label) :
                                     Resource_impl(uuid, label), serviceThread(0) {
    construct();
}

void AudioSink_base::construct()
{
    Resource_impl::_started = false;
    loadProperties();
    serviceThread = 0;
    
    PortableServer::ObjectId_var oid;
    audio_in = new BULKIO_dataShort_In_i("audio_in", this);
    oid = ossie::corba::RootPOA()->activate_object(audio_in);

    registerInPort(audio_in);
}

/*******************************************************************************************
    Framework-level functions
    These functions are generally called by the framework to perform housekeeping.
*******************************************************************************************/
void AudioSink_base::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException)
{
}

void AudioSink_base::start() throw (CORBA::SystemException, CF::Resource::StartError)
{
    boost::mutex::scoped_lock lock(serviceThreadLock);
    if (serviceThread == 0) {
        audio_in->unblock();
        serviceThread = new ProcessThread<AudioSink_base>(this, 0.1);
        serviceThread->start();
    }
    
    if (!Resource_impl::started()) {
    	Resource_impl::start();
    }
}

void AudioSink_base::stop() throw (CORBA::SystemException, CF::Resource::StopError)
{
    boost::mutex::scoped_lock lock(serviceThreadLock);
    // release the child thread (if it exists)
    if (serviceThread != 0) {
        audio_in->block();
        if (!serviceThread->release(2)) {
            throw CF::Resource::StopError(CF::CF_NOTSET, "Processing thread did not die");
        }
        serviceThread = 0;
    }
    
    if (Resource_impl::started()) {
    	Resource_impl::stop();
    }
}

CORBA::Object_ptr AudioSink_base::getPort(const char* _id) throw (CORBA::SystemException, CF::PortSupplier::UnknownPort)
{

    std::map<std::string, Port_Provides_base_impl *>::iterator p_in = inPorts.find(std::string(_id));
    if (p_in != inPorts.end()) {

        if (!strcmp(_id,"audio_in")) {
            BULKIO_dataShort_In_i *ptr = dynamic_cast<BULKIO_dataShort_In_i *>(p_in->second);
            if (ptr) {
                return BULKIO::dataShort::_duplicate(ptr->_this());
            }
        }
    }

    std::map<std::string, CF::Port_var>::iterator p_out = outPorts_var.find(std::string(_id));
    if (p_out != outPorts_var.end()) {
        return CF::Port::_duplicate(p_out->second);
    }

    throw (CF::PortSupplier::UnknownPort());
}

void AudioSink_base::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
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

    delete(audio_in);
 
    Resource_impl::releaseObject();
}

void AudioSink_base::loadProperties()
{
    addProperty(equalizer,
               "equalizer",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(volume,
                1, 
               "volume",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(mute,
                false, 
               "mute",
               "",
               "readwrite",
               "",
               "external",
               "configure");

}
