#!/bin/bash

cp mwait_module/mwait.c mwait_deploy/mwait.c &&
rsync -r mwait_deploy root@$1:/root/ &&
echo "cd mwait_deploy && make" | ssh root@$1 'bash -s'