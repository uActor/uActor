function receive(message)
  if(message.type == "init") then
    additional_size = 8192
    math.randomseed(now()*1379)
    for i=1,3 do
      math.random()
    end
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 1000 + math.random(0, 199))
  end
  if(message.type == "trigger") then

    publication = {type="fake_sensor_value", value=0.1*(math.random(0, 400))}
    if (additional_size > 0) then
      local elem_256 = "A"
      for x=1,8 do
        elem_256 = elem_256 .. elem_256
      end
      local buffer = ""
      for x=1, additional_size / 256 do
        buffer = buffer .. elem_256
      end
      publication["payload"] = buffer
    end

    collectgarbage()
    publish(publication)
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="trigger"}, 200)
  end
end