function receive(message)
  
  code = [[function receive(message)
if(message.type == "init") then
dummy_state = "dummy"
publish(Publication.new("exp_1", "pong"))
end
end]]

  if(message.type == "init") then
    count_sent = 0
    count_received = 0
    sub_id = subscribe({exp_1="pong"})
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
      "deployment_ttl", 999999999
    ))
    testbed_log_integer("count_sent", count_sent)
  elseif(message.exp_1 == "pong") then
    count_received = count_received + 1
    testbed_log_integer("count_received", count_received)
    enqueue_wakeup(1000, "trigger")
  end
end