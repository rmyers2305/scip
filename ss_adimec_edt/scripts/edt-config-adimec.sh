#!/bin/bash

################################################################################
# Define variables
################################################################################
EDT_CAMERA_FILE=/opt/EDTpdv/camera_config/adimec.cfg
INPUT=0

if [[ $# == 0 ]]; then
    echo
    echo Usage: $0 camera_config_file.cfg
    echo
    echo For the Adimec, look in scip/ss_adimec_edt/config/EDT
    echo for config files to try. Try the 1600cd_10cl file first.
    echo
    exit 1
fi
    


if [[ $# -ge 1 ]]; then
   EDT_CAMERA_FILE=$1
fi

if [[ ! -r $EDT_CAMERA_FILE ]]; then
  echo
  echo Cannot read Adimec configuration file $EDT_CAMERA_FILE. 
  echo
  exit 0
fi  



################################################################################
# Print descriptive message
################################################################################
echo -e "\nConfiguring EDT VisionLink F4 Capture Card"



################################################################################
#
# Configure input
#
################################################################################
/opt/EDTpdv/initcam -f ${EDT_CAMERA_FILE} -c ${INPUT}


