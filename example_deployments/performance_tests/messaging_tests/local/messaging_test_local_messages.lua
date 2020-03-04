MAX_COUNT = 100

function receive(message)

  if(message.type == "ping") then
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
    iteration = 0
    count = 0
  end

  if(message.type == "init" or message.type == "setup") then
      
    iteration = 0
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
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
  end

  if(message.type == "trigger") then
    receive_count = 0
    
    local publication = {node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="ping"}

    iteration = iteration + 1

    collectgarbage()
    testbed_start_timekeeping(1)
    for i=1,count do
      publication["n"] = tostring(i)
      publish(publication)
    end

  end
end