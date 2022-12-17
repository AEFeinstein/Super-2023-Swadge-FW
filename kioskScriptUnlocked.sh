#!/bin/bash

#while true
#for i in {1..3}
#do
    #./swadge_emulator --start-mode "Swadge Bros" --lock --fullscreen
    ./swadge_emulator --fullscreen
    retval=$?
    echo $retval
    if [ $retval -eq 0 ] 
    then
        echo "Broke Loop"
        #break
    fi
#done
exit