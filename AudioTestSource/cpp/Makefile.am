ossieName = AudioTestSource
bindir = $(prefix)/dom/components/AudioTestSource/cpp/
bin_PROGRAMS = AudioTestSource

xmldir = $(prefix)/dom/components/AudioTestSource/
dist_xml_DATA = ../AudioTestSource.prf.xml ../AudioTestSource.scd.xml ../AudioTestSource.spd.xml

distclean-local:
	rm -f config.*
	rm -rf autom4te.cache
	rm -f acinclude.m4
	rm -f aclocal.m4
	rm -f configure
	rm -f depcomp
	rm -f install-sh
	rm -f ltmain.sh
	rm -f Makefile.in
	rm -f missing
	rm -rf .deps

# Sources, libraries and library directories are auto-included from a file
# generated by the REDHAWK IDE. You can remove/modify the following lines if
# you wish to manually control these options.
include $(srcdir)/Makefile.am.ide
AudioTestSource_SOURCES = $(redhawk_SOURCES_auto)
AudioTestSource_CXXFLAGS = -Wall $(PROJECTDEPS_CFLAGS) $(BOOST_CPPFLAGS) $(INTERFACEDEPS_CFLAGS) $(redhawk_INCLUDES_auto)
AudioTestSource_LDADD = $(PROJECTDEPS_LIBS) $(BOOST_LDFLAGS) $(BOOST_THREAD_LIB) $(INTERFACEDEPS_LIBS) $(redhawk_LDADD_auto)
AudioTestSource_LDFLAGS = -Wall $(redhawk_LDFLAGS_auto)
