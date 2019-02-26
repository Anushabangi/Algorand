#!/bin/sh
DIR="~/Desktop/expodb-bc-docker"

docker stop dev;
docker stop s1;
docker stop s2;
docker stop s3;
docker stop c4;
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

gnome-terminal -- bash -c "echo dev;
  cd $DIR;
  docker exec -it dev bash;
  cd expo;
  ./rundb -nid0;
  exec bash"

gnome-terminal -- bash -c "echo s1;
  cd $DIR;
  docker exec -it s1 ./rundb -nid1;
  exec bash"

gnome-terminal -- bash -c "echo s2;
  cd $DIR;
  docker exec -it s2 ./rundb -nid2;
  exec bash"

gnome-terminal -- bash -c "echo s3;
  cd $DIR;
  docker exec -it s3 ./rundb -nid3;
  exec bash"

gnome-terminal -- bash -c "echo c4;
  cd $DIR;
  docker exec -it c4 ./runcl -nid4;
  exec bash"
