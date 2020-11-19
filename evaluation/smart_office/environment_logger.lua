function receive(message)
  
  if(message.type == "init") then
    subscribe{type="node_environment_info"}
  end

  if(message.type == "node_environment_info") then
    publish(
      Publication.new(
        "type", "data_point",
        "measurement", "environment",
        "values", "relative_humidity,co2,temperature,pressure,num_relative_humidity,num_co2,num_temperature,num_pressure",
        "tags", "node",
        "co2", message.value_co2,
        "temperature", message.value_temperature,
        "relative_humidity", message.value_relative_humidity,
        "pressure", message.value_pressure,
        "num_co2", message.num_co2,
        "num_relative_humidity", message.num_relative_humidity,
        "num_temperature", message.num_temperature,
        "num_pressure", message.num_pressure,
        "node", message.publisher_node_id,
        "timestamp", message.timestamp
      )
    )
  end
end