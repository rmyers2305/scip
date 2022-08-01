#!/bin/bash

function read_responses
{
    prev_token=XXXX
    first_line_display_g=Y
    echo -e "\n\t Someone set us up a response"
    while read resp; do
	start=0
	end=0
	if [ "$resp" != "" ]; then
            echo $resp
	    start=$(expr index "$resp" '|')
	    end=$(expr index "$resp" '[')
	    len=$end-$start-1
	    if test $start -gt 0 && test $end -gt 0
	    then
		token=${resp:$start:$len}
		args=${resp:$end-1}

		if [ "$prev_token" != "$token" ]; then
		    let display_base_row_g=10
		fi

		#### Limit display region 
		if [[ $display_base_row_g -ge 17 ]]; then
		    let display_base_row_g=10
		fi

		display_status $token $args
	    fi
	    prev_token=$token
	fi
 done

}

function display_status
{
    let start=0
    let end=0
    let len=0

    let start=$(expr index "$args" '[')
    let end=$(expr index "$args" ']')
    let len=$end-$start-1

    if test $len -gt 2
    then
	## Strip off the [] delimiters
	args_stripped=${args:start:len}
        #echo $args_stripped

	echo
	case $token in
            "CURRENTGAIN")
		let display_row=$display_base_row_g+2
		tput cup $display_row 0
                echo " " $token " " $args_stripped "                                                 " 
                ;;

            "CURRENTOFFSET")
		let display_row=$display_base_row_g+3
		tput cup $display_row 0
                echo " " $token " " $args_stripped "                                                 " 
                ;;

	    "CURRENTRGB")
		let display_row=$display_base_row_g+4
		tput cup $display_row 0
                echo " " $token " " $args_stripped "                                                 " 
                ;;
        esac
	### Need this so that the command is echoed on the correct line
	tput cup $cursor_row_g 

    fi

}

function _main
{
    echo -e "\n\t Displaying us some statuses"
    if [ "$#" -lt "1" ]; then
	echo
	echo Usage: $0 device_status_base_row
	echo
	exit 1
    fi

    let cursor_row_g=$1

    read_responses
}

_main $@
