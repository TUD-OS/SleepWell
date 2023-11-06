#!/bin/bash

rsync -r mwait_deploy root@$1:/root/ &&
echo "cd mwait_deploy && make" | ssh root@$1 'bash -s'