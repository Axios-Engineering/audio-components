#!/bin/bash

if [ "$1" == "rpm" ]; then
    # A very simplistic RPM build scenario
    if [ -e AudioTestSource.spec ]; then
        mydir=`dirname $0`
        tmpdir=`mktemp -d`
        cp -r ${mydir} ${tmpdir}/AudioTestSource-0.0.1
        tar czf ${tmpdir}/AudioTestSource-0.0.1.tar.gz --exclude=".svn" -C ${tmpdir} AudioTestSource-0.0.1
        rpmbuild -ta ${tmpdir}/AudioTestSource-0.0.1.tar.gz
        rm -rf $tmpdir
    else
        echo "Missing RPM spec file in" `pwd`
        exit 1
    fi
else
    for impl in cpp ; do
        pushd $impl &> /dev/null
        if [ -e build.sh ]; then
            ./build.sh $*
        else
            echo "No build.sh found for $impl"
        fi
        popd &> /dev/null
    done
fi
