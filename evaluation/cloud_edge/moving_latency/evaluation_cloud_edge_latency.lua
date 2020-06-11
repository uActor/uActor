PEER_ID = "node_cloud"

function receive(message)
  if(message.type == "pong1") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 100 == 0) then
      testbed_log_string("done", 1)
    else
      publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"))
    end
  end

  if(message.type == "init") then
    subscribe{type="pong1"}
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    iteration = 0
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 2000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    
    iteration = iteration + 1
    
    testbed_start_timekeeping(1)
    publish(Publication.new("type", "ping1"))
  end
end