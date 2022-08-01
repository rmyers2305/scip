#!/bin/bash
function dump_instructions
{
    clear
    tput cup 0, 0
    echo
    echo 'Press ? to query for status of the Adimec'
    echo 'Press q to quit gracefully '
    echo 'Press ^C to quit ungracefully'
    echo
}


function usage
{
    if [ "$#" -ne "1" ]; then
	echo
	echo "Usage: $0 device_config_file.cfg"
	echo
	echo
	echo "  device_config_file.cfg is the name of the SCIP Adimec controller"
	echo "  device configuration file."
	echo "          "
	echo
	echo "  The script parser will extract the val for the following config-file param:"
	echo
	echo "     id               (device controller id) "
	echo "          "
	echo 
	echo "  For proper device operation with this script, confirm that the following items in the "
	echo "  device configuration file are set properly."
	echo "          "
	echo "     id:              This should be set to a unique "
	echo "                      (up to 3 digit) ID number."
	echo "          "
	echo "     client_tcp_ip:   This should be set to the IP address of"
	echo "                      the UIB that the SSC Finder is connected to."
	echo
	echo "     client_tcp_port: This should be set to the UIB TCP port"
	echo "                      that the SSC Finder is connected to."
	echo "                      (Valid UIB TCP ports: 8000-8006)"
	echo
	echo "     exec_file:       This is the full path to the SCIP Adimec"
	echo "                      controller executable."
	echo
	echo
	exit 1
    fi
}


function main_
{

    usage $@

    if [ "$SCIP_SHELL_ENVIRONMENT_DEFINED" != "YES" ]; then

	echo 
	echo "#######################################################################"
	echo The SCIP shell environment has not been set up
	echo Run the command \"source \$SCIP_INSTALL_DEST/bin/SCIP_SHELL_ENVIRONMENT.sh\"
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

    parse_config_file $@

    echo Finished parsing config file $1

    proc_name=${exec_file_g##*/}
    trap "echo Killing $proc_name; killall $proc_name; exit 1;" SIGINT

    ## This exported global is used by ./eosadimec_command.sh
    ## to position the command display lines in the command terminal.
    ## A "retrofit" -- too much hassle to invoke as a new cmd line
    ## arg, so we'll export it to eosadimec_command.sh as a global instead.
    export device_command_row_export=7

    ### These set the location (row) of the
    ### command prompt (cursor) and device status displays
    ##$ in the terminal window (used by ./eosadimec_display_status.sh)
    let device_status_base_row_g=2
    let device_status_cursor_row_g=2

    ## Arg $1 is the config file
    ./eos_device_tester.sh ./eosadimec_command.sh \
	$exec_file_g $1 pipe_in_$device_id_g pipe_out_$device_id_g $device_id_g \
	./eosadimec_display_status.sh $device_status_base_row_g $device_status_cursor_row_g


    echo '################# TERMINATING ##########################'
}

main_ $@
