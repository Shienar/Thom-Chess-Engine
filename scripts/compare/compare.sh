#!/bin/bash

# Usage:
#   bash compare.sh path/to/engine1.exe path/to/engine2.exe

cd "$(dirname "$0")"
rm -f fastchess.log config.json

ENGINE_ONE=$(basename $1)
ENGINE_TWO=$(basename $2)

ENGINE_ONE_NAME="${ENGINE_ONE%.*}"
ENGINE_TWO_NAME="${ENGINE_TWO%.*}"

fastchess.exe -engine cmd=../../target/$ENGINE_ONE name=$ENGINE_ONE_NAME \
                -engine cmd=../../target/$ENGINE_TWO name=$ENGINE_TWO_NAME \
                -each tc=8+0.08 \
                -each option.Hash=4 \
                -each option.SyzygyPath=C:/Syzygy \
                -rounds 1000 \
                -repeat \
                -tb C:/Syzygy/ \
                -tbpieces 5 \
                -tbadjudicate BOTH \
                -concurrency 8 \
                -recover \
                -ratinginterval 3 \
                -resign movecount=4 score=1000 twosided=true \
                -openings file=UHO_Lichess_4852_v1.epd format=epd \
                -sprt elo0=0 elo1=1 alpha=0.05 beta=0.05 \
                -log file=fastchess.log