set -e

source ./clean.sh
source ./build.sh

CPU=1
MEM=256

clear
echo "nonoOS Debugging Environment"
sleep 2

screen -c debug_screenrc
