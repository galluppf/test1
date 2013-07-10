#!/bin/bash
####
# SpiNNaker Package source file
# Contains variables to compile/run software in the spiNNaker package
#
# This file needs to contain correct values for the following entries:
#
# MY_PYNN: pointer to the PyNN installation directory (this needs to be in the PYTHONPATH, as pacman and the binary_files_generation projects will be installed at the same level as PyNN is)
# MAKEFILE_SPINN_API: select between arm.make.package and gnu.make.package - you will need to configure the appropriate section below. This is used to compile neural kernels in ./src/app_frame/build
####


################## INSTALLATION AND PYTHON PATHS

# MY_PYNN is the directory whre PyNN is installed (choose the one for your distribution or enter it manually) - it will be used to infer the PYTHONPATH. pyNN.spiNNaker will be linked in the pyNN directory, while pacman and binary_files_generation will be linked in pyNN's parent directory.

#MY_PYNN=/usr/lib/python2.6/site-packages/pyNN 			# fedora
MY_PYNN=/usr/local/lib/python2.7/dist-packages/pyNN		# ubuntu

# used by setup to set symbolic links in the nengo directory
MY_NENGO=/home/francesco/Progetti/NEF/nengo-564b031

################# COMPILER CHOICE AND CONFIGURATION

# MAKEFILE_SPINN_API selects which makefile to use for compiling the SpiNN API and the application framework. arm.make.package for ARM, gnu.make.package for GCC
MAKEFILE_SPINN_API='gnu.make.package'

#################################################################################
###### ARM ENV (configure this if you are using ARM CC to compile neural kernels)
#################################################################################

#the LM_LICENSE file is needed if you want to compile things with the ARM compiler
export LM_LICENSE_FILE=${LM_LICENSE_FILE}
set -a

declare -x ARMBIN=/home/amulinks/spinnaker/tools/RVDS40/RVCT/Programs/4.0/400/linux-pentium
declare -x ARMLIB=/home/amulinks/spinnaker/tools/RVDS40/RVCT/Data/4.0/400/lib
declare -x ARMINC=/home/amulinks/spinnaker/tools/RVDS40/RVCT/Data/4.0/400/include/unix
declare -x LM_LICENSE_FILE=$LM_LICENSE_FILE

declare -x PATH=$ARMBIN:$PATH


##############################################################################
###### GCC ENV (configure this if you are using GCC to compile neural kernels)
##############################################################################

# GCCBIN is the directory where arm-none-linux-gnueabi-gcc, arm-none-linux-gnueabi-ld, arm-none-linux-gnueabi-objcopy, arm-none-linux-gnueabi-objdump are. This needs to be added to the PATH variable in order to compile aplx with GNU
declare -x GCCBIN=/opt/arm-none-linux-gnueabi/bin/
declare -x PATH=$GCCBIN:$PATH


################## POINTERS TO SPECIFIC VERSION (only edit this if you know what you are doing)

### The following need to be edited only if you are not pointing to the default version (useful for rollback/testing)
# set the checkoulevel here (can be testing, trunk or branches/branchname). All the software will be downloaded using this path
CHECKOUT_LEVEL=testing

# change this to checkout/update different versions
REVISION=HEAD


################## INTERNAL POINTERS (The following needs NOT to be edited!)

# PACMAN LOCATION IS ADDED TO THE PATH
#MY_PWD=$(readlink -f $0 | xargs dirname)
# getting the directory of the script http://stackoverflow.com/questions/59895/can-a-bash-script-tell-what-directory-its-stored-in
MY_PWD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
echo $MY_PWD

#PACMAN_LOCATION=$(readlink -e $MYPWD/pacman/)
PACMAN_LOCATION=$MY_PWD/pacman/
echo $PACMAN_LOCATION
export PATH=$PATH:$PACMAN_LOCATION:$MY_PWD/pyNN.spiNNaker:

# Pointer to the SpiNNaker API headers and bin files. Leave it as default to use the one in the package
INC_DIR=../../spin1_api/src
LIB_DIR=../../spin1_api/src

