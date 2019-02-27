## Main Manual
All the scripts have the same basic functions but for different situations. These situations including different OS (Mac and Linux), different output formats (stdout or txt file), different ways to start dev service (automatically or manually).
## File Intro
#### runM.sh
This script includes the prepare part and it is the Mac version.
#### runNoLog.sh
This script is the Linux version of runM.sh, which does not generate (or update) log files.
#### runL.sh
This script includes the prepare part and it is the Linux version. It will also write the output of s1, s2, s3, c4 into txt files.
#### prepare.sh
The script will stop and start all the containers and update the IP address according to them. It is the Linux version.
#### runLs.sh
This script only starts the s1, s2, s3, c4. In order to use this one, you will need to run prepare.sh first, and then manually start the dev using the following codes.
```
docker exec -it dev bash;
cd expo;
./rundb -nid0;
exec bash
```
