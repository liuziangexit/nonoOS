set -e

source ./clean.sh
source ./build.sh

clear
echo "nonoOS Debugger"
sleep 2

screen -c debug_screenrc
