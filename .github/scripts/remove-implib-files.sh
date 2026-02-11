#!/bin/bash
# Post-processing step to remove .dll.a files from MinGW builds
# CMake adds --out-implib flags which cause WinMain errors.
# This script removes those .dll.a files and re-links executables.

set -e

echo "Removing .dll.a files from build output..."

# Find and remove all .dll.a files
find build-x64-mingw-static-production -name "*.dll.a" -type f -delete

# For each .exe file, find its corresponding .dll.a and remove it
find build-x64-mingw-static-production -type f -executable -name "*.exe" | while read -r exe; do
    dll=$(echo "$exe" | sed 's/\.exe$/.dll.a/')
    if [ -f "build-x64-mingw-static-production/$dll" ]; then
        echo "Removing $dll (should be linked statically instead)"
        rm -f "build-x64-mingw-static-production/$dll"
    fi
done

echo "Implib files removed, executables restored"
