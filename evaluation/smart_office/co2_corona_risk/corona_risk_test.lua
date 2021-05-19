function receive(message)
  if(message.type == "init") then
    enqueue_wakeup(5000, "person_count")
    enqueue_wakeup(5050, "co2_value")
    count_sent = 0
  end
  if(message.type == "wakeup" and message.wakeup_id == "person_count") then
    num_keys = 0
    if(count_sent >= 60 and count_sent <= 240) then
      num_keys = 2
    end
    publish(Publication.new("type", "node_en_key_count", "num_keys", num_keys))
    count_sent = count_sent + 1
    enqueue_wakeup(100, "person_count")
  end

  if(message.type == "wakeup" and message.wakeup_id == "co2_value") then
    co2 = 800
    if (not ts) then
      ts = 0
    else
      ts = ts + 60
    end
    publish(Publication.new("type", "node_environment_info", "timestamp", ts, "value_co2", co2))
    enqueue_wakeup(100, "co2_value")
  end
end