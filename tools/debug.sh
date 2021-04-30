set -e

source ./clean.sh
source ./build.sh

clear
echo "nonoOS Debugger"
#sleep 1

screen -c debug_screenrc
