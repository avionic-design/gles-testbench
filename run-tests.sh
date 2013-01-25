#!/bin/sh

echo "=============================================="
echo " Avionic Design GL Pipeline Benchmark"
echo "=============================================="

export DISPLAY=:0
export LD_LIBRARY_PATH=/usr/lib

echo "Disable VSYNC, you might be asked for root pw"
echo 1 | sudo tee /sys/module/window/parameters/no_vsync

echo "=============================================="
echo " Test 1: 1 to 1 Texture copy"
./src/gles-standalone copy

echo "=============================================="
echo " Test 2: 1 to All Texture copy"
./src/gles-standalone one_source

echo "=============================================="
echo " Test 3: 3-line Linear blend"
./src/gles-standalone deinterlace

echo "=============================================="
echo " Test 4: GL Blanking (no shaders)"
./src/gles-standalone blank
