#include <iostream>
#include "ossie/ossieSupport.h"

#include "AudioTestSource.h"

 int main(int argc, char* argv[])
{
    AudioTestSource_i* AudioTestSource_servant;
    Resource_impl::start_component(AudioTestSource_servant, argc, argv);
}
