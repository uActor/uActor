sleep 30

for ip in $(python3 $TESTBED_ROOT_DIR/../tools/testbed_ips.py)
do
  python3 $TESTBED_ROOT_DIR/../tools/actorctl.py -f $TESTBED_CONFIG_DIR/$1 ${ip} 1337
done