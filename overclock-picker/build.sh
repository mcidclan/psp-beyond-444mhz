#!/bin/bash

if [ -z "$1" ]; then
    echo "=== Starting build process ==="
else
    VERSION=$1
    echo "=== Starting build process for $VERSION ==="
fi

echo "Building tester..."
make clean
make
if [ $? -ne 0 ]; then
    echo "Error: Tester build failed"
    exit 1
fi
echo "Tester built successfully"

echo "Building plugin..."
cd ./plugin
make clean
make
if [ $? -ne 0 ]; then
    echo "Error: Plugin build failed"
    exit 1
fi
cd ..
echo "Plugin built successfully"

echo ""
echo "=== Copying files ==="
mkdir -p ./bin/build/PSP/GAME/opicker-tester
mkdir -p ./bin/build/SEPLUGINS

echo "Tester Files..."
cp -f ./bin/EBOOT.PBP ./bin/build/PSP/GAME/opicker-tester/
cp -f ./bin/kcall.prx ./bin/build/PSP/GAME/opicker-tester/
echo "EBOOT.PBP, kcall copied"

echo "Copying Plugin..."
cp -f ./plugin/bin/opicker.prx ./bin/build/SEPLUGINS/

echo ""
if [ -z "$1" ]; then
    echo "=== Build completed successfully ==="
else
    echo "=== Build completed successfully for $VERSION ==="
fi
