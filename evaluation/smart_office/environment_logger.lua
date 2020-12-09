function receive(message)
  
  if(message.type == "init") then
    subscribe{type="node_environment_info"}
  end

  if(message.type == "node_environment_info") then
    publish(
      Publication.new(
        "type", "data_point",
        "measurement", "environment",
        "values", values_for(message),
        "tags", tags_for(message),
        "co2", message.value_co2,
        "temperature", message.value_temperature,
        "relative_humidity", message.value_relative_humidity,
        "pressure", message.value_pressure,
        "num_co2", message.num_co2,
        "num_relative_humidity", message.num_relative_humidity,
        "num_temperature", message.num_temperature,
        "num_pressure", message.num_pressure,
        "sensor_co2", message.sensor_co2,
        "sensor_relative_humidity", message.sensor_relative_humidity,
        "sensor_temperature", message.sensor_temperature,
        "sensor_pressure", message.sensor_pressure,
        "node", message.publisher_node_id,
        "timestamp", message.timestamp
      )
    )
  end
end

function tags_for(message)
  tags = "node,"
  if(message.sensor_relative_humidity) then
    tags = tags.."sensor_relative_humidity,"
  end
  if(message.sensor_temperature) then
    tags = tags.."sensor_temperature,"
  end
  if(message.sensor_pressure) then
    tags = tags.."sensor_pressure,"
  end
  if(message.sensor_co2) then
    tags = tags.."sensor_co2,"
  end
  return string.sub(tags, 1, -2)
end

function values_for(message)
  tags = ""
  if(message.sensor_relative_humidity) then
    tags = tags.."relative_humidity,num_relative_humidity,"
  end
  if(message.sensor_temperature) then
    tags = tags.."temperature,num_temperature,"
  end
  if(message.sensor_pressure) then
    tags = tags.."pressure,num_pressure,"
  end
  if(message.sensor_co2) then
    tags = tags.."co2,num_co2,"
  end
  return string.sub(tags, 1, -2)
end