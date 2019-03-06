#!/bin/bash
# !!! For reference only, don't run !!!
echo "This shell is for reference only, please don't run it."
exit 0

# Setup
./setup.sh

# Get all ip address
docker inspect s1
docker inspect s2
docker inspect s3
docker inspect c4

# Create ifconfig.txt and put iP address in it.
# under ExpoDB-BC

# Run
./run.sh

##############################################################

# Future Work
# Multi-VM
# With Oracle VirtualBox
docker-machine create --driver virtualbox myvm1
docker-machine create --driver virtualbox myvm2

# To see the machine
docker-machine ls
# To see the configuration
docker-machine env myvm1

# configure shell to talk to myvm1
eval $(docker-machine env myvm1)
docker stack deploy -c docker-compose.yml getstartedlab

##############################################################
