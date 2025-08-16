#!/bin/bash

make clean
make
chmod +x build/ipv4-chat

./build/ipv4-chat 255.255.255.255 8000                                           