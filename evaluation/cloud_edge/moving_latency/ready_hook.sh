
ROOT_DIR="/home/pi/experiment_assets/hetzel/uActor"

ssh cm-pi1 "mkdir -p $ROOT_DIR && rm -rf $ROOT_DIR/* && mkdir $ROOT_DIR/tools && mkdir $ROOT_DIR/assets"
scp $TESTBED_ROOT_DIR/tools/* cm-pi1:$ROOT_DIR/tools
scp $TESTBED_CONFIG_DIR/* cm-pi1:$ROOT_DIR/assets
ssh -R 192.168.50.3:1337:127.0.0.1:1337 -R $TESTBED_SERVER_PORT:$TESTBED_SERVER_HOST:$TESTBED_SERVER_PORT cm-pi1 << EOF
  bash --login -c "echo 'called' && python3 $ROOT_DIR/tools/peer_announcer.py && echo "done" && sleep 30 && python3 $ROOT_DIR/tools/peer_announcer.py && sleep 30 && python3 $ROOT_DIR/tools/actorctl.py \$(python3 $ROOT_DIR/tools/testbed_ip_for.py node_1) 1337 $ROOT_DIR/assets/evaluation_cloud_edge_latency_$1.yml"
EOF