set -e

source ./clean.sh
source ./build.sh

clear
echo "nonoOS Debugging Environment"
sleep 2

screen -c debug_screenrc
