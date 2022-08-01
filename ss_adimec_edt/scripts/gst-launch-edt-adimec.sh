#!/bin/bash
## Minimal example gst-launch script for the edt capture card
## Works on the test RHEL8 system (as of 2022/03/09)

echo
echo If this script fails to stream video from the Adimec camera,
echo run scip/ss_adimec_edt/scripts/edt-config-adimec.sh and try again.
echo

sleep 3

gst-launch-1.0 edtpdvsrc ! queue ! videoconvert ! autovideosink
