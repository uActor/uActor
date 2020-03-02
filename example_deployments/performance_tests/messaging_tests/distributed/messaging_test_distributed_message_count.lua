PEER_ID = "node_8"

MAX_COUNT = 120

function receive(message)
  if(message.type == "pong") then
    receive_count = receive_count + 1
    if(receive_count == count) then
      testbed_stop_timekeeping(1, iteration_name)
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 5000 + math.random(0, 199))
    end
  end

  if(message.type == "init") then
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    count = 0
    iteration = 0
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 5000)
  end
  
  if(message.type == "trigger") then
    receive_count = 0

    if(iteration % 10 == 0) then
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
      iteration_name = "latency_messages_"..tostring(count)
      publish_iteration_name = "publish_messages_"..tostring(count)
    end

    publication = {node_id=PEER_ID, actor_type="test_echo_actor", instance_id="test_deployment_echo", type="ping"}

    iteration = iteration + 1

    collectgarbage()
    testbed_start_timekeeping(1)

    -- testbed_start_timekeeping(3) -- breakdown measurement
    for i=1,count do
      publication["n"] = tostring(i)
      publish(publication)
    end
    -- testbed_stop_timekeeping(3, publish_iteration_name) -- breakdown measurement
  end
end