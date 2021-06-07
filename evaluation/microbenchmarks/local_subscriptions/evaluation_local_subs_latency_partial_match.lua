MAX_SUB_COUNT = 800
COUNT_INCREMENTS = 10

function receive(message)

  if(message.exp_1 == "ping") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 25 == 0) then
      enqueue_wakeup(1000 + math.random(0, 199), "setup")
    else
      enqueue_wakeup(1000 + math.random(0, 199), "trigger")
    end
  end

  if(message.type == "init") then
    subscribe({exp_1="ping", exp_2="foo"})
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    iteration = 0
    count = -COUNT_INCREMENTS
  end

  if(message.type == "init" or (message.type == "wakeup" and message.wakeup_id == "setup")) then
    iteration = 0
    count = count + COUNT_INCREMENTS
    if(count > MAX_SUB_COUNT) then
      testbed_log_string("done" , "true")
      return
    end

    if(count > 0) then
      for x=1,COUNT_INCREMENTS,1 do
        local base_id = count - COUNT_INCREMENTS
        local sub_id = base_id + x
        local distribution_string = string.char(string.byte("a") + (sub_id-1) % 26)
        local sub = {exp_1="ping", exp_2=distribution_string.."_v_"..sub_id}
        subscribe(sub)
      end
    end

    testbed_log_string("_logger_test_postfix", count)

    collectgarbage()
    enqueue_wakeup(1000 + math.random(0, 199), "trigger")
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then
    iteration = iteration + 1

    local publication = Publication.new("exp_1", "ping", "exp_2", "foo")
    
    collectgarbage()
    testbed_start_timekeeping(1)
    
    publish(publication)
  end
end