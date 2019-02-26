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
