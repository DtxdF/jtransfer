#!/bin/sh

#
# Script designed to be run for development purposes only.
#

"${SUEXEC:-doas}" make JTRANSFER_VERSION=`make -V JTRANSFER_VERSION`+`git rev-parse HEAD`
