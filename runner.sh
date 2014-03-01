#!/bin/bash

# Copyright (c) 2014 Stanford University
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

set -e

count=1000
iters=5
machines=$(ls machines)
sizes=$(for i in {0..20}; do echo $(( 2**$i )); done)
offsets=$(for i in {0..31}; do echo $(( 512*$i )); done)

cp header.csv results.csv

for iter in $(seq $iters); do
    for machine in $machines; do
        source machines/$machine
        sendfile bench.c
        cmd gcc -Wall -lrt bench.c -o bench
        for disk in $disks; do
            disk=($(echo $disk | tr : ' '))
            diskname=${disk[0]}
            diskdevice=${disk[1]}
            diskmount=${disk[2]}
            echo "Testing $machine $diskname"
            for writecache in yes no; do
                if [ $writecache = "yes" ]; then
                    rootcmd hdparm -W 1 /dev/$diskdevice
                else
                    rootcmd hdparm -W 0 /dev/$diskdevice
                fi
                for size in $sizes; do
                for offset in $offsets; do
                for direct in yes no; do
                    if [ $direct == "yes" -a $size -lt 512 ]; then
                        continue
                    fi
                    run="./bench "
                    run="$run --file=$diskmount/bench.dat "
                    run="$run --size=$size "
                    run="$run --count $count "
                    run="$run --offset=$offset "
                    run="$run --direct=$direct"
                    #echo "$run"
                    out=$(cmd "$run")
                    seconds=$(echo $out | cut -d' ' -f2)
                    if [ -n "$seconds" ]; then
                        (echo -n "$machine,$diskname,$writecache,"
                         echo -n "$size,$offset,$direct,$count,"
                         echo "$seconds") >> results.csv
                    fi
                done # direct
                done # offset
                done # size
            done # writecache
            rootcmd hdparm -W 1 /dev/$diskdevice
        done # disk
    done # machine
done
