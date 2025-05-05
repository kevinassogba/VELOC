#!/bin/bash

install_dir=$1
statediff_dir="/home/kta7930/research/anl/state-diff"
kokkos_dir="/home/kta7930/research/anl/install/kokkos/installcpu/lib/cmake/Kokkos"

./auto-install.py $install_dir $statediff_dir $kokkos_dir