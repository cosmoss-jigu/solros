#!/bin/bash

# load common config
CURDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source $CURDIR/config.sh

# install perf to the system for the Ubuntu distribution 
full_version=`uname -r`
sudo mkdir -p "/usr/lib/linux-tools/$full_version"
sudo mkdir -p "/usr/lib/linux-tools-$full_version"
sudo cp ${SRC_ROOT}/host-kernel/tools/perf/perf "/usr/lib/linux-tools-$full_version/"
sudo rm -f "/usr/lib/linux-tools/$full_version/perf"
sudo ln -s "/usr/lib/linux-tools-$full_version/perf"  "/usr/lib/linux-tools/$full_version/perf"
