DIR="~/Desktop/expodb-bc-docker"

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
