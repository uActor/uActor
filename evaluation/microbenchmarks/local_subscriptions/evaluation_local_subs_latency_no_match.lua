MAX_SUB_COUNT = 800

function receive(message)

  if(message.foo == "bar") then
    testbed_stop_timekeeping(1, "latency")
    if(iteration % 25 == 0) then
      enqueue_wakeup(1000 + math.random(0, 199), "setup")
    else
      enqueue_wakeup(1000 + math.random(0, 199), "trigger")
    end
  end

  if(message.type == "init") then

    subscribe({foo="bar", baz=qux})
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    iteration = 0
    count = - 1
  end

  if(message.type == "init" or (message.type == "wakeup" and message.wakeup_id == "setup")) then

    iteration = 0
    
    if(count == -1) then
      count = 0
    else
      count = count + 4
    end

    if(count > MAX_SUB_COUNT) then
      testbed_log_string("done" , "true")
      return
    end

    if count > 0 then
      local sub = {qux="bar"}
      sub["baz_1_"..count] = "value_1_"..count
      subscribe(sub)
      sub = {qux="bar"}
      sub["baz_2_"..count] = "value_2_"..count
      subscribe(sub)
      sub = {qux="bar"}
      sub["baz_3_"..count] = "value_3_"..count
      subscribe(sub)
      sub = {qux="bar"}
      sub["baz_4_"..count] = "value_4_"..count
      subscribe(sub)
    end

    testbed_log_string("_logger_test_postfix", count)

    collectgarbage()
    enqueue_wakeup(1000 + math.random(0, 199), "trigger")
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then
    iteration = iteration + 1

    local publication = Publication.new("foo", "bar", "baz", "qux")
    
    collectgarbage()
    testbed_start_timekeeping(1)
    
    publish(publication)
  end
end