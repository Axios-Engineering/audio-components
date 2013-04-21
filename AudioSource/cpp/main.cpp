#include <iostream>
#include "ossie/ossieSupport.h"

#include "AudioSource.h"

 int main(int argc, char* argv[])
{
    AudioSource_i* AudioSource_servant;
    Resource_impl::start_component(AudioSource_servant, argc, argv);
}
