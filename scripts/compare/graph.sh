#!/bin/bash

# bash graph.sh path/to/engine1.exe path/to/engine2.exe
cd "$(dirname "$0")"
bash compare.sh $1 $2 | py -3.14 graph.py