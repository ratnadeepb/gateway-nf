#!/bin/bash
# wget https://download.redis.io/releases/redis-6.2.1.tar.gz
# tar xzf redis-6.2.1.tar.gz
# cd redis-6.2.1
# make
# sudo apt install tcl
# make test

# run the script with sudo
add-apt-repository ppa:redislabs/redis
apt update
apt install redis