#!/bin/sh

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker stop dev;
    docker start dev;
    docker exec -it dev bash;
    cd expo;
    ./rundb -nid0"
end tell'

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker stop s1;
    docker start s1;
    docker exec -it s1 ./rundb -nid1"
end tell'

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker stop s2;
    docker start s2;
    docker exec -it s2 ./rundb -nid2"
end tell'

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker stop s3;
    docker start s3;
    docker exec -it s3 ./rundb -nid3"
end tell'

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker stop c4;
    docker start c4;
    docker exec -it c4 ./runcl -nid4"
end tell'
