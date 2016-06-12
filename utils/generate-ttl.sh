#!/bin/bash

set -e

if [ -d bin ]; then
  cd bin
else
  echo "Please run this script from the source root folder"
  exit
fi

PWD=`dirname $0`

if [ -f $PWD/lv2_ttl_generator32.exe ]; then
  GEN=$PWD/lv2_ttl_generator32.exe
  EXT=dll
  OSX=
  WIN32=1
  WIN64=
elif [ -f $PWD/lv2_ttl_generator64.exe ]; then
  GEN=$PWD/lv2_ttl_generator64.exe
  EXT=dll
  OSX=
  WIN32=
  WIN64=1
elif [ -f $PWD/lv2_ttl_generatorosx ]; then
  GEN=$PWD/lv2_ttl_generatorosx
  EXT=dylib
  OSX=1
  WIN32=
  WIN64=
else
  GEN=$PWD/lv2_ttl_generator
  EXT=so
fi

FOLDERS=`find . -type d -name \*.lv2`

for i in $FOLDERS; do
  cd $i
  FILE=`ls *.$EXT | sort | head -n 1`
  if [ "x$WIN64" == "x1" ]; then
    #wine64 $GEN ./$FILE
    echo "Not generating ttl for WIN64 yet"
  elif [ "x$WIN32" == "x1" ]; then
    #wine $GEN ./$FILE
    echo "Not generating ttl for WIN32 yet"
  elif [ "x$OSX" == "x1" ]; then
    echo "Not generating ttl for OSX yet"
  else
    $GEN ./$FILE
  fi
  cd ..
done
