#!/usr/bin/sh

#
# Typical version identifier is x.y.z, e.g. 1.0.4
# the version number is then prepended with 'v' for
# the tags annotation in git.
#
VERSION=$1
if git tag -l | grep -w v$VERSION; then
    echo Git tag already exists
	exit 1
fi

#
# Clean up any previously attempted archives
#
rm sliderule-icesat2-$VERSION.tar.gz 2> /dev/null

#
# Update version in local repository
#
echo $VERSION > version.txt
git add version.txt
git commit -m "Version v$VERSION"

#
# Create tag and acrhive
#
git tag -a v$VERSION -m "version $VERSION"
git archive --format=tar.gz --prefix=sliderule-icesat2/ v$VERSION > sliderule-icesat2-$VERSION.tar.gz

