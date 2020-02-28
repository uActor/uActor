PEER_ID = "node_8"

MAX_SIZE_FACTOR = 14

function receive(message)

  if(message.type == "pong") then
    testbed_stop_timekeeping(1, iteration_name)
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 5000)
    factor = -1
    iteration = 0
  end
  
  if(message.type == "trigger") then

    if(iteration % 10 == 0) then
      iteration = 0
      factor = factor + 1
      if(factor > MAX_SIZE_FACTOR) then
        testbed_log_string("done" , "true")
        return
      end
      if(factor > 0) then
        iteration_name = "latency_size_"..tostring(2^(factor-1))
        publish_iteration_name = "publish_size_"..tostring(2^(factor-2))
      else
        iteration_name = "latency_size_"..tostring(0)
        publish_iteration_name = "publish_size_"..tostring(0) 
      end
    end

    publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"}
    
    if (factor > 0) then
      local buffer = "A"
      for x=2,factor do
        buffer = buffer .. buffer
      end
      publication["payload"] = buffer
    end

    iteration = iteration + 1

    collectgarbage()
    testbed_start_timekeeping(1)

    -- testbed_start_timekeeping(3) -- breakdown measurement
    publish(publication)
    -- testbed_stop_timekeeping(3, publish_iteration_name) -- breakdown measurement
  end
end