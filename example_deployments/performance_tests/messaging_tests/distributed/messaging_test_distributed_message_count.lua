PEER_ID = "node_1"

MAX_COUNT = 120

function receive(message)
  if(message.type == "pong") then
    receive_count = receive_count + 1
    if(receive_count == count) then
      testbed_stop_timekeeping(1, "latency")
      if(iteration % 10 == 0) then
        delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="setup"}, 1000 + math.random(0, 199))
      else
        delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
      end
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    count = 0
    iteration = 0
  end
  
  if(message.type == "init" or message.type == "setup") then
    if(count == 0) then
      count = 1
    elseif(count == 1) then
      count = 5
    else
      count = count + 5
    end
    if(count > MAX_COUNT) then
      testbed_log_string("done" , "true")
      return
    end
    
    testbed_log_string("_logger_test_postfix", count)
    collectgarbage()
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 2000 + math.random(0, 199))
  end

  if(message.type == "trigger") then

    iteration = iteration + 1
    
    receive_count = 0

    local publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"}

    collectgarbage()
    testbed_start_timekeeping(1)
    for i=1,count do
      publication["n"] = tostring(i)
      publish(publication)
    end
  end
end