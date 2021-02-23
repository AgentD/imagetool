#!/bin/sh

COVERITY_PATH="$HOME/Downloads/cov-analysis-linux64-2020.09"
TOKEN=$(cat "$HOME/.coverity/imgtool")
EMAIL=$(git log --follow --pretty=format:"%ae" -- coverity.sh | head -n 1)

DESCRIPTION=$(git describe --always --tags --dirty)
VERSION=$(grep AC_INIT configure.ac | grep -o \\[[0-9.]*\\] | tr -d [])

export PATH="$COVERITY_PATH/bin:$PATH"

./autogen.sh
./configure
make clean
cov-build --dir cov-int make -j
tar czvf imagetool.tgz cov-int

echo "Uploading tarball with the following settings:"
echo "Email: $EMAIL"
echo "Version: $VERSION"
echo "Description: $DESCRIPTION"

curl --form token="$TOKEN" \
     --form email="$EMAIL" \
     --form file=@imagetool.tgz \
     --form version="$VERSION" \
     --form description="$DESCRIPTION" \
     https://scan.coverity.com/builds?project=AgentD%2Fimagetool
