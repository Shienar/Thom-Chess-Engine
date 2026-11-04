#!/bin/bash
# Format: compile-commits.sh <commit-count>
# OR
# Format: compile-commits.sh <newer-commit-offset-from-head> <older-commit-offset-from-head> 
# e.g. compile-commits.sh 5 10

cd "$(dirname "$0")"

ENGINE_NAME="Thom"
OUTPUT_DIR="$PWD/../../target"
TEMP_DIR="$PWD/temp/"
MAKEFILE_OUTPUT="Thom.exe"   

if [ -n "$2" ]; then
older_commit_num=$2
newer_commit_num=$1
echo "Collecting hashes for commits $newer_commit_num-$older_commit_num from head..."
COMMITS=$(git rev-list HEAD~$older_commit_num..HEAD~$newer_commit_num)
depth=$older_commit_num
counter=$(git rev-list --all --count)
counter=$((counter - $newer_commit_num))
else
echo "Collecting hashes for the last $1 commits..."
COMMITS=$(git rev-list -n $1 HEAD) 
depth=$1
counter=$(git rev-list --all --count)
fi

mkdir -p "$TEMP_DIR"
mkdir -p "$OUTPUT_DIR"
cd $TEMP_DIR

git clone --depth $depth https://github.com/Shienar/Thom-Chess-Engine
cd Thom-Chess-Engine

for HASH in $COMMITS; do
    SHORT_HASH=$(git rev-parse --short $HASH)
    EXE_NAME="${counter}-${ENGINE_NAME}-${SHORT_HASH}.exe"
    
    echo "Checking out commit: $SHORT_HASH"
    
    git checkout -f $HASH > /dev/null 2>&1
    
    echo -e "\tCompiling..."
    make -j TARGET=$MAKEFILE_OUTPUT > /dev/null 2>&1
    
    if [ -f "$MAKEFILE_OUTPUT" ]; then
        echo -e "\tSuccess! Saving to ${OUTPUT_DIR}/${EXE_NAME}"
        mv "$MAKEFILE_OUTPUT" "${OUTPUT_DIR}/${EXE_NAME}"
    else
        echo -e "\tERROR: Compilation failed."
    fi

    make clean TARGET=$MAKEFILE_OUTPUT > /dev/null 2>&1

    ((counter--))
done

echo "Finished compilations"
cd $TEMP_DIR
cd ..
rm -rf $TEMP_DIR
echo "Finished Cleanup."