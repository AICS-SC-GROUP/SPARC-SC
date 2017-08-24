#!/bin/bash
k=0.0625
sub='tt'
for i in `seq 7`
do
    echo $i' s='$k
    ./nbody.out -i -t 100.0 -R 0.1 -o 0.0625 -D $k -e 0.0 input/p3t.input 1>result/nbody.log.p3t.$sub$i 2>&1 
    mv nbody.dat result/nbody.dat.p3t.$sub$i
    k=`echo 'scale=10; '$k'/2'|bc`
done