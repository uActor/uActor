function receive(message)
  if(message.type == "init" or message.type == "trigger") then
    publish(Publication.new("type", "fake_sensor_value", "value", 0.1*(math.random(0, 400))))
    if(counter == nil or counter < 600) then
      delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "trigger"), 1300)
    else
      testbed_log_integer("done", 1)
    end
    if(message.type == "init") then
      counter = 1
    else
      counter = counter + 1
    end
  end
end