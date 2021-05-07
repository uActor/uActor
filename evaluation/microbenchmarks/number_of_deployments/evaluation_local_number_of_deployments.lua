function receive(message)
  
  code = [[function receive(message)
    if(message.type == "init") then
      dummy_state = "dummy"
      publish(Publication.new("type", "pong"))
    end
  end]]

  if(message.type == "init") then
    count_sent = 0
    count_received = 0
    sub_id = subscribe({type="pong"})
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 1000)
    enqueue_wakeup(1000, "trigger")
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then
    count_sent = count_sent + 1
    publish(Publication.new(
      "type", "deployment",
      "deployment_name", "benchmark.spawn_deployment_"..count_sent,
      "deployment_actor_type", "benchmark.spawn_actor",
      "deployment_actor_runtime_type", "lua",
      "deployment_actor_version", "0.1",
      "deployment_actor_code", code,
      "deployment_required_actors", "",
      "deployment_ttl", 9999999
    ))
    testbed_log_integer("count_sent", count_sent)
  elseif(message.type == "pong") then
    count_received = count_received + 1
    publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "wakeup", "wakeup_id", "trigger"))
    testbed_log_integer("count_received", count_received)
  end
end