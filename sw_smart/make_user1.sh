#########################################################################
# File Name: eps_make.sh
# Author: rlingxing
# mail: rlingxing@163.com
# Created Time: 2016年06月03日 星期五 11时45分29秒
#########################################################################
#!/bin/bash

export SDK_PATH=$(dirname $(pwd))
export BIN_PATH=$(dirname $(pwd))/bin

echo ""

echo "start..."
echo ""

make clean

make BOOT=new APP=1 SPI_SPEED=2 SPI_MODE=0 SPI_SIZE_MAP=2

mv ../bin/upgrade/user1.1024.new.2.bin  ../bin/upgrade/user1_.bin

echo "generate user1_.bin"

rm ../bin/upgrade/*.dump

