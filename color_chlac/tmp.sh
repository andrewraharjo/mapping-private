#!/bin/bash

for noise in noiseless noisy
do
    for shape in cone cube cylinder dice plane sphere torus
    do
	for color in black blue green orange purple red yellow
	do
	    #rosrun color_chlac convertOld2NewPCD demos/shape_data/${noise}_${shape}_${color}.pcd demos/shape_data/${noise}_${shape}_${color}.pcd
	    rosrun color_chlac test_GRSD_CCHLAC demos/shape_data/${noise}_${shape}_${color}.pcd

	    #git add demos/shape_data/${noise}_${shape}_${color}.pcd
	    #git add demos/shape_data/${noise}_${shape}_${color}_GRSD_CCHLAC.pcd
	done
    done
done