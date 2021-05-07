function receive(message)
  
  code = [[function receive(message)
    if(message.type == "init") then
      publish({node_id="]]..node_id..[[", actor_type="spawn_test_host", instance_id="spawn_test_deployment", type="pong"})
    end
  end]]

  if(message.type == "init") then
    count_sent = 0
    count_received = 0
  end

  if(message.type == "init" or message.type == "trigger") then
    count_sent = count_sent + 1
    publish({
      type="deployment",
      deployment_name="benchmark.spawn_deployment_"..count_sent,
      deployment_actor_type="benchmark.spawn_actor",
      deployment_actor_runtime_type="lua",
      deployment_actor_version="0.1",
      deployment_actor_code=code,
      deployment_required_actors="core.io.gpio",
      deployment_ttl=1000000
    })
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 5000)
    testbed_log_integer("count_sent", count_sent)
  elseif(message.type == "pong") then
    count_received = count_received + 1
    testbed_log_integer("count_received", count_received)
  end
end