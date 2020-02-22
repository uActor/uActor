NUM_NODES = 3
HOST_NODE = "node_home"

function receive(message)
  code = [[function receive(message)
    if(message.type == "init") then
      publish({node_id="]]..HOST_NODE..[[", actor_type="spawn_test_host", instance_id="spawn_test_deployment", data="pong"})
    end
  end]]
  if(message.type == "init" and node_id == HOST_NODE) then
    -- wait until the subscription for the backwards pass is forwarded to all nodes 
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="ready"}, 1000)
  elseif(message.type == "ready") then
    start = now()
    count = 0
    publish({
      type="deployment",
      deployment_name="benchmark.spawn_deployment", deployment_actor_type="benchmark.spawn_actor", deployment_actor_runtime_type="lua",
      deployment_actor_version="0.1",
      deployment_actor_code=code,
      deployment_required_actors="core.io.gpio",
      deployment_ttl=5000
    })
  elseif(node_id == HOST_NODE and message.data == "pong") then
    t= now() - start
    testbed_log_integer("time_"..message.publisher_node_id, t)
    testbed_log_integer("time", t)
    count = count + 1
    if(count == NUM_NODES) then
      count = 0
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="ready"}, 6000) 
    end
  end
end