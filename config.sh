#!/bin/bash
set -euf -o pipefail


finish_suggest() {
	echo ''
	echo 'OpenWRT already configured'
	echo 'Use make menuconfig to customize further'
	echo 'Use make -j$(nproc) command to build image'
	echo ''
	exit 0
}

if [ -f .config ]; then
	finish_suggest
fi

./scripts/feeds update -a -f
./scripts/feeds install -a -f

cp -v config.radxa-zero-3e .config

finish_suggest
