#!/bin/bash


function usage
{
    if [ "$#" -ne "1" ] && [ "$#" -ne "2" ]; then
	echo
	echo "Usage: $0 device_config_file.cfg [scip-server-ip-address]"
	echo
	echo
	echo "  device_config_file.cfg is the name of the EOS Adimec controller"
	echo "  device configuration file."
	echo "          "
	echo "          "
	echo "  The script parser will extract vals for the following config-file params:"
	echo "          "
	echo "     id               (device controller id) "
	echo "          "
	echo "     listening_port   (SCIP server port to connect to) "
	echo "          "
	echo "          "
	echo "  For proper EOS Adimec controller operation, confirm that the following items in the "
	echo "  device configuration file are set properly."
	echo
	echo "     id:              This should be set to a unique "
	echo "                      (up to 3 digit) ID number."
	echo
	echo "     listening_port:  This should be set to an open SCIP"
	echo "                      server listening port"
	echo
	echo "          "
	echo "  The optional scip-server-ip-address arg can be used to override"
	echo "  the default \"localhost\" SCIP server ip-address"
	echo
	echo
	exit 1
    fi
}

function dump_instructions
{
    clear
    tput cup 0, 0
    echo 'Press g to query the digital gain'
    echo 'Press o to query the offset'
    echo 'Press w to query the RGB balance'
    echo 'Press R to reset the terminal display'
    echo 'Press q to quit gracefully '
    echo 'Press ^C to quit ungracefully'
    echo
    tput cup $command_base_row_g 0
    tput sc
}




function _main
{
    usage $@

    if [ "$SCIP_SHELL_ENVIRONMENT_DEFINED" != "YES" ]; then
	echo 
	echo "#######################################################################"
	echo The SCIP shell environment has not been set up
	echo Run the command \"source \$SCIP_INSTALL_DEST/SCIP/bin/SCIP_SHELL_ENVIRONMENT.sh\"
	echo to set up the SCIP environment, and then relaunch this script.
	echo "#######################################################################"
	echo
	exit 1;
    fi


    if [ ! -e $1 ]; then
	echo
	echo "  The configuration file $1 does not exist. "
	echo
	exit 1
    fi


    error_flag=0

    if [ ! -e ./parse_config_file.sh ]; then
	echo
	echo "  The parse_config_file.sh script file is missing."
	echo "  It can be found in ~/SVN/SCIP/trunk/common/scripts/."
	echo "  Copy parse_config_file.sh to this local directory."
	echo
	error_flag=1
    fi

    if [ ! -e ./eosadimec_command.sh ]; then
	echo
	echo "  The eosadimec_command.sh command generator script file is missing."
	echo "  It can be found in ~/SVN/SCIP/trunk/ss_adimec/scripts/."
	echo "  Copy eosadimec_command.sh to this local directory."
	echo
	error_flag=1
    fi

    if [ ! -e ./eosadimec_display_status.sh ]; then
	echo
	echo "  The eosadimec_display_status.sh Adimec status script file is missing. "
	echo "  It can be found in ~/SVN/SCIP/trunk/ss_adimec/scripts/."
	echo "  Copy eosadimec_display_status.sh to this local directory."
	echo
	error_flag=1
    fi

    if [ ! -e ./eos_device_tester.sh ]; then
	echo
	echo "  The eos_device_tester.sh wrapper script file is missing."
	echo "  It can be found in ~/SVN/SCIP/trunk/common/scripts/."
	echo "  Copy eos_device_tester.sh to this local directory."
	echo
	error_flag=1
    fi

    if [ "$error_flag" = "1" ]; then
	echo
	echo Error:
	echo "    Cannot execute this script until the above shell-script files  "
	echo "    are copied to " $PWD
	echo
	echo "    Don't forget to chmod +x all script files, i.e.:"
	echo "        chmod +x *.sh"
	exit 1
    fi

    source ./parse_config_file.sh

    let command_base_row_g=10
    let device_status_base_row_g=10

    dump_instructions

    parse_config_file $1
    
    scip_ip_address="localhost"

    ## Override "localhost" as the IP address
    ## of the SCIP server (for remote access)
    if [ "$#" -gt "1" ]; then
	scip_ip_address=$2
    fi

    export device_command_row_export=8

    . ./eosadimec_command.sh $device_id_g | nc $scip_ip_address $listening_port_g | \
	. ./eosadimec_display_status.sh $device_status_base_row_g
}

_main $@
