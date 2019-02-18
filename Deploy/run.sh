#!/bin/sh

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker exec -it s1 ./rundb -nid1"
end tell'

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker exec -it s2 ./rundb -nid2"
end tell'

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker exec -it s3 ./rundb -nid3"
end tell'

osascript -e 'tell app "Terminal"
    do script "cd Downloads/expodb-bc-docker;
    docker exec -it c4 ./runcl -nid4"
end tell'
