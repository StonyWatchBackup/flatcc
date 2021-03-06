#!/usr/bin/env bash

set -e

echo "removing build products"

cd `dirname $0`/..

rm -rf build
rm -rf release
rm -f bin/flatcc*
rm -f bin/bfbs2json*
rm -f lib/libflatcc*
rmdir bin
rmdir lib
