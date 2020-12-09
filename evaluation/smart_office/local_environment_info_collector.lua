VARIABLES = {
  temperature={unit="degree_celsius", sensor="bme280", alt_sensor="scd30"},
  co2={unit="ppm", sensor="scd30"},
  relative_humidity={unit="percent", sensor="bme280", alt_sensor="scd30"},
  pressure={unit="pa", sensor="bme280"}
}

function receive(message)
  
  local current_seconds, current_nanos = unix_timestamp()
  local current_minute = current_seconds // 60
  
  if(not value_store) then
    value_store = {}
  end

  if(not value_store[current_minute]) then
    value_store[current_minute] = {}
    for variable, configuration in pairs(VARIABLES) do
      value_store[current_minute][variable] = {
        sum = 0,
        count = 0
      }
    end
  end

  if(message.type == "init") then
    alt_sub_ids = {}
    for variable, configuration in pairs(VARIABLES) do
      subscribe{type="sensor_update_"..variable, publisher_node_id=node_id, sensor=configuration.sensor}
      if(configuration.alt_sensor) then
        alt_sub_ids[variable] = subscribe{type="sensor_update_"..variable, publisher_node_id=node_id, sensor=configuration.alt_sensor}
      end
    end

    delayed_publish(
      Publication.new(
      "node_id", node_id,
      "actor_type", actor_type,
      "instance_id", instance_id,
      "type", "trigger_calculate_average"
      ),
      (60-current_seconds%60)*1000
    )

  end

  for variable, configuration in pairs(VARIABLES) do
    if(message.type == "sensor_update_"..variable) then
      local current = value_store[current_minute][variable]
      if(message.sensor == configuration.sensor or alt_sub_ids[variable]) then
        if(message.sensor == configuration.sensor and alt_sub_ids[variable]) then
          unsubscribe(alt_sub_ids[variable])
          alt_sub_ids[variable] = nil
          current.sum = 0
          current.count = 0
        end
        current.sum = current.sum + message.value
        current.count = current.count + 1
      end
    end
  end

  if(message.type == "trigger_calculate_average") then
    for minute, data in pairs(value_store) do
      if(minute < current_minute) then
        local publication = Publication.new("type", "node_environment_info", "timestamp", minute*60)
        local should_publish = false

        for variable, _configuration in pairs(VARIABLES) do
          if(data[variable]["count"] > 0) then
            should_publish = true
            publication["value_"..variable] = data[variable]["sum"] / data[variable]["count"]
            publication["num_"..variable] = data[variable]["count"]
            publication["sensor_"..variable] = sensor_for(variable)
          end
        end
        value_store[minute] = nil
        
        if(should_publish) then
          publish(publication)
        end
      end
    end

    delayed_publish(
      Publication.new(
      "node_id", node_id,
      "actor_type", actor_type,
      "instance_id", instance_id,
      "type", "trigger_calculate_average"
      ),
      60000
    )

  end
end

function sensor_for(variable)
  if alt_sub_ids[variable] then
    return VARIABLES[variable].alt_sensor
  else
    return VARIABLES[variable].sensor
  end
end