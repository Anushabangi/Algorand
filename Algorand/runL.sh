#!/bin/sh
DIR="/root/expodb-bc-docker"

#docker stop dev;
#docker stop s1;
#docker stop s2;
#docker stop s3;
#docker stop c4;
docker start dev;
docker start s1;
docker start s2;
docker start s3;
docker start c4;

devIP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' dev)
s1IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' s1)
s2IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' s2)
s3IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' s3)
c4IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' c4)
echo "$devIP\n$s1IP\n$s2IP\n$s3IP\n$c4IP" > "ExpoDB-BC/ifconfig.txt"

(bash -c "echo s1;
  cd $DIR;
  docker exec -i s1 ./rundb -nid1 > logs1.txt;
  exec bash")&

(bash -c "echo s2;
  cd $DIR;
  docker exec -i s2 ./rundb -nid2 > logs2.txt;
  exec bash")&

(bash -c "echo s3;
  cd $DIR;
  docker exec -i s3 ./rundb -nid3 > logs3.txt;
  exec bash")&

(bash -c "echo c4;
  cd $DIR;
  docker exec -i c4 ./runcl -nid4 > logc4.txt;
  exec bash")&

sudo docker exec -it dev ./expo/dev_run.sh