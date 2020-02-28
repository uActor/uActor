PEER_ID = "node_8"

EXTRA_ELEMENT_COUNTS =  {0, 1, 2, 4, 8, 16, 32, 64, 256, 384}

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
    count_index = 0
    iteration = 0
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 10000)
  end
  
  if(message.type == "trigger") then
    if(iteration % 1 == 0) then
      iteration = 0
      count_index = count_index + 1
      if(count_index > #EXTRA_ELEMENT_COUNTS) then
        testbed_log_string("done" , "true")
        return
      end
      iteration_name = "latency_elements_"..EXTRA_ELEMENT_COUNTS[count_index]
      publish_iteration_name = "publish_elements_"..EXTRA_ELEMENT_COUNTS[count_index]
    end

    publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"}
      
    buffer = "ABCD"
    for x=1,EXTRA_ELEMENT_COUNTS[count_index],1 do
      if(x % 7 == 0) then
        publication["dummy_int_"..tostring(x)] = x
      elseif(x % 8 == 0) then
        publication["dummy_float_"..tostring(x)] = 0.1*x
      else
        publication["dummy_"..tostring(x)] = buffer
      end
    end

    iteration = iteration + 1

    buffer = nil
    collectgarbage()

    testbed_start_timekeeping(1)

    -- testbed_start_timekeeping(3) -- breakdown measurement
    publish(publication)
    -- testbed_stop_timekeeping(3, publish_iteration_name) -- breakdown measurement
  end
end