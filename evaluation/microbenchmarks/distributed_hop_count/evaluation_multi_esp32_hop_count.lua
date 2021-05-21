MAX_DISTANCE = 19

function receive(message)

  if(message.type == "pong") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 25 == 0) then
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "setup"), 5000 + math.random(0, 199))
    else
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 1000 + math.random(0, 199))
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    distance = 20
    iteration = 0
  end
  
  if(message.type == "init" or message.type == "setup") then

    iteration = 0
    distance = distance - 1
    
    if(distance < 0) then
      testbed_log_string("done" , "true")
      return
    end

    testbed_log_string("_logger_test_postfix", distance)
    collectgarbage()
    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 5000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    iteration = iteration + 1
    
    local receiver_id = "node_"..tostring(distance+1)

    collectgarbage()
    testbed_start_timekeeping(1)
    publish(Publication.new("node_id", receiver_id, "actor_type", "test_echo_actor", "instance_id", "test_deployment_echo", "type", "ping"))
  end
end