For the EDT card, the adimec camera configuration file and
the scip_video.x configuration file do not specify any Adimec 
.cam configuration files.

Instead, you run an EDT configuration utility, supplied with the 
name of the configuration file per below.

To configure the EDT to work with the Adimec 1600cd camera, do the following:
(The other .cam files don't speciy the color-table correctly for the Adimec_1600cd camera
currently in use).

$ /opt/EDTpdv/initcam -f /full/path/to/admimec1600cd_10cl_bayer.cam -c 0


This will configure the EDT card to allow adimec camera camlink serial port and 
the EDTpdv gstreamer plugin edtpdvsrc to work.
