PEER_ID = "node_1"

EXTRA_ELEMENT_COUNTS =  {0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 384}

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
    count_index = 0
    iteration = 0
  end

  if(message.type =="init" or message.type == "setup") then
    iteration = 0
    count_index = count_index + 1
    if(count_index > #EXTRA_ELEMENT_COUNTS) then
      testbed_log_string("done" , "true")
      return
    end

    testbed_log_string("_logger_test_postfix", EXTRA_ELEMENT_COUNTS[count_index])
    collectgarbage()
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 2000 + math.random(0, 199))
  end
  
  if(message.type == "trigger") then
    iteration = iteration + 1
    local publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"}
      
    for x=1,EXTRA_ELEMENT_COUNTS[count_index],1 do
      if(x % 7 == 0) then
        publication["dummy_int_"..tostring(x)] = x
      elseif(x % 8 == 0) then
        publication["dummy_float_"..tostring(x)] = 0.1*x
      else
        publication["dummy_"..tostring(x)] = "ABCD"
      end
    end

    collectgarbage()
    testbed_start_timekeeping(1)
    publish(publication)
  end
end