PEER_ID = "node_1"

MAX_SIZE_FACTOR = 14

function receive(message)

  if(message.type == "pong") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 10 == 0) then
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="setup"}, 1000 + math.random(0, 199))
    else
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    factor = -1
    iteration = 0
  end
  
  if(message.type == "init" or message.type == "setup") then

    iteration = 0
    factor = factor + 1
    if(factor > MAX_SIZE_FACTOR) then
      testbed_log_string("done" , "true")
      return
    end
    
    if(factor > 0) then
      testbed_log_string("_logger_test_postfix", tostring(2^(factor-1)))
    else
      testbed_log_string("_logger_test_postfix", 0)
    end

    collectgarbage()
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 5000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    
    iteration = iteration + 1
    
    local publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"}
    
    if (factor > 0) then
      local buffer = "A"
      for x=2,factor do
        buffer = buffer .. buffer
      end
      publication["payload"] = buffer
    end


    collectgarbage()
    testbed_start_timekeeping(1)
    publish(publication)
  end
end