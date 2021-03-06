MAX_DISTANCE = 19
NODE_IDS = {"96A679", "979E51", "73C324", "F57BF0", "B75EAC", "B759EC", "B75B58", "B78AE8", "6C1BF4", "9446E9", "E3B4D0", "E3B5E0", "E3AB24", "E3A868", "E3A848", "E3B494", "E3A7E8", "E3B6B8", "E3B4A4", "E3ABB8"}

function receive(message)
  if(message.exp_1 == "pong") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 50 == 0) then
      enqueue_wakeup(5000 + math.random(0, 199), "setup")
    else
      enqueue_wakeup(1000 + math.random(0, 199), "trigger")
    end
  end

  if(message.type == "init") then
    -- Return path uses the unique default subscription; No explicit subscription is need.
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    distance = 20
    iteration = 0
  end
  
  if(message.type == "init" or (message.type == "wakeup" and message.wakeup_id == "setup")) then

    iteration = 0
    distance = distance - 1
    
    if(distance < 0) then
      testbed_log_string("done" , "true")
      return
    end

    testbed_log_string("_logger_test_postfix", distance)
    collectgarbage()
    enqueue_wakeup(5000 + math.random(0, 199), "trigger")
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then
    iteration = iteration + 1

    collectgarbage()
    testbed_start_timekeeping(1)
    publish(Publication.new("node_id", NODE_IDS[distance+1], "exp_1", "ping")) 
  end
end