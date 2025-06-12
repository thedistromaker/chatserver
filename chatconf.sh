#!/bin/bash

buildchatd() {
if [ -e $(pwd)/chatd ]; then
        echo "ALREADY COMPILED"
        exit 1
fi

gcc -Wall -Wextra -pthread client.c -lncursesw -o client
gcc -Wall -pthread chatd.c -o chatd
}

case $1 in
        install)
                sudo cp --dereference chatd /usr/local/bin/
                sudo cp --dereference client /usr/local/bin/
                echo "Installed."
                exit 0
                ;;
        build)
                echo "Starting build..."
                buildchatd
                ;;
        clean)
                rm -vf chatd client
                echo "Cleaned."
                ;;
        configure)
                echo "Edit parameters in nano."
                echo "You MUST rebuild after reconfiguring."
                nano config.h
                ;;
        *)
                echo "Usage: ./build.sh install|build|clean|configure"
                exit 1
                ;;
esac
