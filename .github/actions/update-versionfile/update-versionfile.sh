#!/bin/bash -l

echo "versionfile updater. looking for tag…"
version=$(git describe --always --tag)

versionfilePath="./addons/main/script_version.hpp"

IFS='.-' read -ra versionbits <<< ${version}
major=${versionbits[0]}
minor=${versionbits[1]}
patch=${versionbits[2]}
build=${versionbits[3]}
commit=$(echo ${versionbits[4]} | tr -d g)

re='^[0-9]+$'
if ! [[ ${major} =~ $re && ${minor} =~ $re && ${patch} =~ $re ]] ; then
   echo "not a release tag: '$version', not writing versionfile."
   exit
fi

echo "we're on version $major.$minor.$patch, build $build (commit $commit ). updating versionfile…"

printf "#define MAJOR $major\n#define MINOR $minor\n#define PATCHLVL $patch\n#define BUILD 0" > $versionfilePath

echo "Version file is now:"
echo $(cat $versionfilePath)