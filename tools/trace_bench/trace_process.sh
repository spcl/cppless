##!/bin/bash

for var in "$@"
do
    echo "$var"
    python3 tools/aws_trace/aws_trace.py "$var" > ${var%.json}.txt
done