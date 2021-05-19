function receive(message)
  print("receive")
  if(message.type == "init") then
    room_data={paper_example_room={width=4.2, depth=3, height=3, ventilation_rate=2.3, co2_baseline=400}}
    subscribe({type="fetch_room_data"})
  end

  if(message.type == "fetch_room_data") then
    if (room_data[message.room_id] == nil) then
      return
    end
    data = room_data[message.room_id]

    publish(Publication.new(
      "node_id", message.publisher_node_id,
      "actor_type", message.publisher_actor_type,
      "instance_id", message.publisher_instance_id,
      "type", "room_configuration_data",
      "room_id", message.room_id,
      "room_width", data["width"],
      "room_depth", data["depth"],
      "room_height", data["height"],
      "room_ventilation_rate", data["ventilation_rate"],
      "room_co2_baseline", data["co2_baseline"],
      "risk_tolerance", 0.1,
      "relative_suscceptibility", 1.0
    ))
  end
end