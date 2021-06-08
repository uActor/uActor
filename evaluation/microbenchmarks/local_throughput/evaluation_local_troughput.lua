function receive(message)
  if(message.exp_1 == "ping") then
    count = count + 1
    if(count < 10000) then
      publish(Publication.new("exp_1", "ping"))
    else
      testbed_stop_timekeeping(1, "time_for_10000")
      count = 0
      enqueue_wakeup(1000, "start_run")
    end
  elseif(message.type == "init") then
    subscribe({exp_1="ping"})
    count = 0
    enqueue_wakeup(10000, "start_run")
  elseif(message.type == "wakeup" and message.wakeup_id == "start_run") then
    collectgarbage()
    testbed_start_timekeeping(1)
    publish(Publication.new("exp_1", "ping"))
  end
end