#!/bin/bash

### Save the stdout file descriptor
exec 5>&1

### Redirect stdout to stderr
exec >&2


## $device_command_row_export must be defined and exported 
## in the master script that invokes this script (sscfinder_command.sh)

if [ "$device_command_row_export" = "" ]; then
    echo
    echo Must define set device_command_row_export to a reasonable integer
    echo in the routine that calls this script \($0\)
    echo
    exit 1
fi

if [ "$#" -ne "2" ] && [ "$#" -ne "1" ]; then
   echo  
   echo "Usage: $0 dev_id [pipe_to_device_controller] " 
   echo 
   exit 1
fi

pipe_to_device_controller=""

if [ "$#" -eq "2" ]; then
    pipe_to_device_controller=$2
    if [ ! -p "$2" ]; then
	echo 
	echo Creating named-pipe $2 
	echo 
	mkfifo $2
    fi
fi


## $1 could have a leading 0, causing bash to think that
## it's an octal value. Force to base10 with no leading 0s.
dev_id_oct=$1
dev_id_10=$((10#$dev_id_oct)) ## In case of leading 0(octal), force decimal representation
dev_id=$dev_id_10


clear 

tput cup 0, 0 
echo 'Press g to query the digital gain' 
echo 'Press s to query the offset'
echo 'Press w to query the RGB balance' 
echo 'Press R to reset the terminal display'
echo 'Press q to quit gracefully ' 
echo 'Press ^C to quit ungracefully' 
echo 
tput cup $device_command_row_export  0 
tput sc       

### Restore stdout
###exec 1>&5 5>&-

while [ 1 ]; do

    tput cup $device_command_row_export 0 
    
    read -n 1 cmd
    
    valid_command=0
    quit_flag=0

    
  case $cmd in

      'g') 
          echo "                                              " 
	  tput cup $device_command_row_export 0                                          
	  echo -n "Querying digital gain        "      
	  eosadimec_cmd=$(echo SS`printf "%3.3d" $dev_id`'|'getgain[])
	  valid_command=1
	  let display_base_row_g=10
	  ;;

        's')
            echo "                                             "
           tput cup $comman_display_row_export 0
           echo -n "Setting digital gain       "
           eosadimec_cmd=$(echo SS`printf "%3.3d" $dev_id`'|'setgain[500])
           valid_command=1
           let display_base_row_g=10
           ;;

      'o') 
	  tput cup $device_command_row_export 0                                        
          echo "                                            " 
	  tput cup $device_command_row_export 0                                        
	  echo "Querying offset                                "
	  eosadimec_cmd=$(echo SS`printf "%3.3d" $dev_id`'|'getoffset[])
	  valid_command=1
	  let display_base_row_g=12
	  ;;

      'w')
           tput cup $device_command_row_export 0
           echo "                                            "
           tput cup $device_command_row_export 0
           echo "Querying RGB balance                            "
           eosadimec_cmd=$(echo SS`printf "%3.3d" $dev_id`'|'getrgb[])
           valid_command=1
           let display_base_row_g=12
           ;;

      'R') 
	  clear 
	  
	  tput cup 0, 0 
	  echo 'Press g to query the digital gain' 
	  echo 'Press o to query the offset'
	  echo 'Press w to query the RGB balance' 
	  echo 'Press R to reset the terminal display'
	  echo 'Press q to quit gracefully ' 
	  echo 'Press ^C to quit ungracefully' 
	  echo 
	  tput cup $device_command_row_export  0 
	  tput sc       
	  let display_base_row_g=12
	  export display_base_row_g
	  ;;

      'q')
	  tput cup $device_command_row_export 0                                         
	  echo "                                              "
	  tput cup $device_command_row_export 0                                         
	  quit_flag="1";
	  ;;

       *)
	  valid_command=0
	  ;;

  esac


  echo quitflag $quit_flag
  echo validflag $valid_command

  if [ "$quit_flag" = "1" ]; then
      break;
  fi

  if [ "$valid_command" = "1" ]; then
      echo EOSadimec command: "$eosadimec_cmd" '            '
      if [ "$pipe_to_device_controller" = "" ]; then
	  tput cup 10 0                                     
	  echo '                                           '
	  tput cup 10 0                                     

	  ## The echo $eosadimec_cmd below must go to standard output (not std error)
	  ## We want to restore stdout just long enough to send
	  ## out a SCIP command (which we may want to redirect
	  ## to the device controller process)
	  ## Want this to go to stdout, not stderr
	  ## Restore stdout, close fd5 (where stdout was stashed)
	  exec 1>&5 5>&-
	  echo ' '$eosadimec_cmd'                           '

          ### Save the stdout file descriptor
          ### to fd5
          exec 5>&1

          ### Redirect stdout back to stderr
          exec >&2
      else
	  echo ' '$eosadimec_cmd' ' > $pipe_to_device_controller
      fi
  fi

done

### Want everything below will go to stderr


tput rc 

echo              
echo "Exiting..." 
echo              
