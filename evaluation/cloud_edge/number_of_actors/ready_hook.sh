ROOT_DIR="/home/pi/experiment_assets/hetzel/uActor"

ssh cm-pi1 "mkdir -p $ROOT_DIR && rm -rf $ROOT_DIR/* && mkdir $ROOT_DIR/tools && mkdir $ROOT_DIR/assets"
scp $TESTBED_ROOT_DIR/tools/* cm-pi1:$ROOT_DIR/tools
scp $TESTBED_CONFIG_DIR/* cm-pi1:$ROOT_DIR/assets

ssh -R 192.168.50.3:1337:192.168.50.3:1337 -R $TESTBED_SERVER_PORT:$TESTBED_SERVER_HOST:$TESTBED_SERVER_PORT cm-pi1 << EOF
  bash --login -c 'for ip in \$(python3 $ROOT_DIR/tools/testbed_ips.py); do python3 tools/actorctl.py \${ip} 1337 $ROOT_DIR/assets/$1; done'
EOF