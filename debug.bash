#!/usr/bin/env bash
set -e

make main-debug
./main-debug
gprof main-debug > report.txt
