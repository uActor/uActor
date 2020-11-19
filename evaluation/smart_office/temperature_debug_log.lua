function receive(message)
  if(message.type == "init") then
    subscribe({type="sensor_update_temperature", unit="degree_celsius"})
  end
  if(message.type == "sensor_update_temperature") then
    publish(
      Publication.new(
        "type", "data_point",
        "measurement", "temperature_debug",
        "values", "temperature",
        "tags", "node,sensor",
        "temperature", message.value,
        "node", message.publisher_node_id,
        "sensor", message.sensor
      )
    )
  end
end