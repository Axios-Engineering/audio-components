#include <iostream>
#include "ossie/ossieSupport.h"

#include "AudioSink.h"

 int main(int argc, char* argv[])
{
    AudioSink_i* AudioSink_servant;
    Resource_impl::start_component(AudioSink_servant, argc, argv);
}
