PEER_ID = "node_2"
MAX_COUNT = 100

function receive(message)
  if(message.type == "pong") then
    receive_count = receive_count + 1
    if(receive_count == MAX_COUNT) then
      testbed_stop_timekeeping(1, "latency")
      if(iteration % 10 == 0) then
        delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="setup"}, 1000 + math.random(0, 199))
      else
        delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
      end
    elseif (send_count < MAX_COUNT) then
      send_count = send_count + 1
      local publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping", send_count=send_count}
      collectgarbage()
      publish(publication)
    else
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
    
    iteration = 0
   
    count = count + 1
    -- if(count == 0) then
    --   count = 1
    -- else
    --   count = count + 19
    -- end

    if(count > 50) then
      testbed_log_string("done" , "true")
      return
    end
    
    testbed_log_string("_logger_test_postfix", count)
    collectgarbage()
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 5000 + math.random(0, 199))
  end

  if(message.type == "trigger") then

    iteration = iteration + 1
    send_count = 0
    receive_count = 0

    local publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"}

    collectgarbage()
    testbed_start_timekeeping(1)
    for i=1,count do
      send_count = send_count + 1
      publication["send_count"] = send_count
      publish(publication)
    end
  end
end