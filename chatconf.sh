#!/bin/bash

buildchatd() {
        if [ -e $(pwd)/chatd ]; then
                echo "W: Already built, exiting."
                exit 0
        fi
        ceho "Building client..."
        gcc -Wall -Wextra -pthread client.c -lncursesw -o client
        echo "Building chatd..."
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
                echo "Usage: $0 install|build|clean|configure"
                exit 1
                ;;
esac
