#!/bin/bash

install_dir=$1
statediff_dir="/home/keveltun/research/recup/veloc/apps/state-diff"
kokkos_dir="/home/keveltun/install/kokkos/installcpu/lib64/cmake/Kokkos"

./auto-install.py $install_dir $statediff_dir $kokkos_dir
