##!/bin/bash

start=0
end=13

tile_w=("16")
tile_h=("32")

for (( c=0; c<${#tile_w[@]}; c++ ))
do
    echo $c
    for (( i=start; i<end; i++ ))
    do
	    echo "${tile_w[c]} ${tile_h[c]} $i"
        ./build/bench/benchmarks/custom/ray/benchmark_custom_ray_cli "-w" "512" "-a" "1" "-s" "200" "-d" "20" "--dispatcher" "--dispatcher-tile-width" "${tile_w[c]}" "--dispatcher-tile-height" "${tile_h[c]}" "--dispatcher-trace-output" "output/trace/ray.${tile_w[c]}-${tile_h[c]}.$i.json" >/dev/null
    done
done