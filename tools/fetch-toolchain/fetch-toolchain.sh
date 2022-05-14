#!/bin/bash


mkdir -p $3
docker build -f $1 -t $2 $(dirname $1)
docker run --rm $2 tar --dereference -czf - /lib /usr/include /usr/lib /usr/local/lib | tar -xzf - -C $3