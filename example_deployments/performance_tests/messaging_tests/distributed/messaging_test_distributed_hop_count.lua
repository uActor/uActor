MAX_DISTANCE = 15

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
    distance = -1
    iteration = 0
  end
  
  if(message.type == "trigger") then

    if(iteration % 10 == 0) then
      iteration = 0
      distance = distance + 1
      if(distance > MAX_DISTANCE) then
        testbed_log_string("done" , "true")
        return
      end
      iteration_name = "latency_distance_"..tostring(distance)
      publish_iteration_name = "publish_distance_"..tostring(distance)
    end

    local receiver_id = "node_"..tostring(distance+1)

    iteration = iteration + 1

    collectgarbage()
    testbed_start_timekeeping(1)

    -- testbed_start_timekeeping(3) -- breakdown measurement
    publish({node_id=receiver_id, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"})
    -- testbed_stop_timekeeping(3, publish_iteration_name) -- breakdown measurement
  end
end