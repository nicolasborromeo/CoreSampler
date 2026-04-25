#!/bin/bash
set -e
CMAKE=/Applications/CMake.app/Contents/bin/cmake
APP="build/CoreSampler_artefacts/Debug/Standalone/Core Sampler.app"

cd "$(dirname "$0")"
echo "Building..."
$CMAKE --build build --config Debug
echo "Launching..."
# Kill any running instance, wait a moment, then launch fresh
pkill -x "Core Sampler" 2>/dev/null || true
sleep 0.5
open -n "$APP"
