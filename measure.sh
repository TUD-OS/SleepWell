#!/bin/bash

(echo "mwait_deploy/measure.sh" | ssh root@$1 'bash -s') &&
rm -rf output/results &&
rsync -r root@$1:/root/mwait_deploy/results/ output/results/ &&
python3 scripts/evaluateMeasurements.py output/results/