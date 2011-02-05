#!/bin/bash
export KERNEL_DIR=$PWD
export ARCH=arm
export CROSS_COMPILE=arm-eabi-
echo -n "Enter full path to arm-eabi- (ex, ~/<Android source dir>/prebuilt/linux-x86/toolchain/arm-eabi-4.4.0/bin ) [ENTER]: "
read fullpath
export PATH=$PATH:$fullpath
echo KERNEL_DIR=$KERNEL_DIR
echo ARCH=$ARCH
echo CROSS_COMPILE=$CROSS_COMPILE
echo PATH=$PATH
