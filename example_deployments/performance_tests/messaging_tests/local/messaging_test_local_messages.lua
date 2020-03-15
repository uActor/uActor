MAX_COUNT = 101

function receive(message)
  if(message.type == "ping") then
    -- testbed_stop_timekeeping_inner(7, "lua")
    receive_count = receive_count + 1
    if(receive_count == MAX_COUNT) then
      testbed_stop_timekeeping(1, "latency")
      if(iteration % 25 == 0) then
        delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="setup"}, 1000 + math.random(0, 199))
      else
        delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
      end
    elseif (send_count < MAX_COUNT) then
      -- testbed_start_timekeeping(8)
      send_count = send_count + 1
      local publication = {node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="ping", send_count=send_count}
      collectgarbage()
      publish(publication)
      -- testbed_stop_timekeeping_inner(8, "pub")
    else
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
    count = count + 1

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
    send_count = 0
    
    local publication = {node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="ping"}

    iteration = iteration + 1

    collectgarbage()
    testbed_start_timekeeping(1)
    -- testbed_start_timekeeping(8)
    for i=1,count do
      send_count = send_count + 1
      publication["send_count"] = send_count
      publish(publication)
    end
    -- testbed_stop_timekeeping_inner(8, "pub")

  end
end