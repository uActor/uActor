ROOT_DIR="/home/pi/experiment_assets/hetzel/uActor"

ssh cm-pi1 "mkdir -p $ROOT_DIR && rm -rf $ROOT_DIR/* && mkdir $ROOT_DIR/tools && mkdir $ROOT_DIR/assets && mkdir $ROOT_DIR/evaluation_tools"
scp $TESTBED_ROOT_DIR/../tools/* cm-pi1:$ROOT_DIR/tools
scp $TESTBED_ROOT_DIR/../evaluation/tools/* cm-pi1:$ROOT_DIR/evaluation_tools
scp $TESTBED_CONFIG_DIR/* cm-pi1:$ROOT_DIR/assets

echo $TESTBED_SERVER_PORT
echo $TESTBED_SERVER_HOST

sleep 30

ssh -R $TESTBED_SERVER_PORT:$TESTBED_SERVER_HOST:$TESTBED_SERVER_PORT cm-pi1 << EOF
  bash --login -c 'for ip in \$(python3 $ROOT_DIR/evaluation_tools/testbed_ips.py); do python3 $ROOT_DIR/tools/actorctl.py \${ip} 1337 -f $ROOT_DIR/assets/$1; done'
EOF