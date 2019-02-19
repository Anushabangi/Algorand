#!/bin/sh
DIR="Downloads/expodb-bc-docker"
xterm -hold -e "cd $DIR;
    docker stop dev;
    docker start dev;
    sudo docker exec -it dev bash;
    cd expo;
    ./rundb -nid0"

xterm -hold -e "cd $DIR;
    docker stop s1;
    docker start s1;
    docker exec -it s1 ./rundb -nid1"

xterm -hold -e "cd $DIR;
    docker stop s2;
    docker start s2;
    docker exec -it s2 ./rundb -nid2"

xterm -hold -e "cd $DIR;
    docker stop s3;
    docker start s3;
    docker exec -it s3 ./rundb -nid3"

xterm -hold -e "cd $DIR;
    docker stop c4;
    docker start c4;
    docker exec -it c4 ./runcl -nid4"
