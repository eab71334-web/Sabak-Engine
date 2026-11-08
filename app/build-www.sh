#!/bin/sh
# Copies the engine + demo into app/www (the web folder the iOS wrapper loads)
set -e
cd "$(dirname "$0")"
rm -rf www && mkdir -p www
cp -R ../src ../demo ../assets www/
printf '<meta http-equiv="refresh" content="0;url=demo/index.html">' > www/index.html
