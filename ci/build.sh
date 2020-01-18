#!/usr/bin/env bash

set -eux

sudo apt-get update

sudo apt-get install -y python3 python3-pip gperf ninja-build ca-certificates wget dpkg

wget https://github.com/Kitware/CMake/releases/download/v3.16.2/cmake-3.16.2-Linux-x86_64.sh
sudo sh ./cmake-3.16.2-Linux-x86_64.sh --skip-license --prefix=/usr
rm cmake-3.16.2-Linux-x86_64.sh

wget https://github.com/protocolbuffers/protobuf/releases/download/v3.11.2/protoc-3.11.2-linux-x86_64.zip
sudo unzip -o protoc-3.11.2-linux-x86_64.zip -d /usr
rm protoc-3.11.2-linux-x86_64.zip

wget http://mirrors.kernel.org/ubuntu/pool/main/d/device-tree-compiler/device-tree-compiler_1.4.7-1_amd64.deb
sudo dpkg -i device-tree-compiler_1.4.7-1_amd64.deb
rm device-tree-compiler_1.4.7-1_amd64.deb

toolchain_dir="$HOME/toolchain"
if [ ! -d "${toolchain_dir}" ]; then
    mkdir -p "${toolchain_dir}"

    pushd "${toolchain_dir}"
    curl -L "https://developer.arm.com/-/media/Files/downloads/gnu-rm/7-2018q2/gcc-arm-none-eabi-7-2018-q2-update-linux.tar.bz2?revision=bc2c96c0-14b5-4bb4-9f18-bceb4050fee7?product=GNU%20Arm%20Embedded%20Toolchain,64-bit,,Linux,7-2018-q2-update" | tar xvj
    popd
fi

pyenv global 3.5.2

python -m venv "$HOME/venv"
set +eux
. "$HOME/venv/bin/activate"
set -eux

pip install --upgrade pip
pip install --upgrade setuptools


export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH="${toolchain_dir}/gcc-arm-none-eabi-7-2018-q2-update"

pip install west

ncs_dir="$HOME/ncs"
rm -rf "${ncs_dir}"
mkdir -p "${ncs_dir}"
cd "${ncs_dir}"
west init -m https://github.com/mwudka/fw-nrfconnect-nrf.git
west update

pip install -r "${ncs_dir}/zephyr/scripts/requirements.txt"
pip install -r "${ncs_dir}/nrf/scripts/requirements.txt"
pip install -r "${ncs_dir}/mcuboot/scripts/requirements.txt"

pip install --upgrade protobuf

source "${ncs_dir}/zephyr/zephyr-env.sh"

build_dir="/tmp/build"
rm -rf "${build_dir}"
mkdir -p "${build_dir}"
cd "${build_dir}"
cmake -GNinja -DBOARD=nrf9160_pca20035ns "$HOME/project/firmware"
ninja