VARIABLES = {
  temperature={unit="degree_celsius", sensor="bme280"},
  co2={unit="ppm", sensor="scd30"},
  relative_humidity={unit="percent", sensor="bme280"},
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
      value_store[current_minute][variable] = {}
    end
  end

  if(message.type == "init") then
    for variable, configuration in pairs(VARIABLES) do
      subscribe{type="sensor_update_"..variable, unit=configuration.unit, publisher_node_id=node_id, sensor=configuration.value}
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

  for variable, _configuration in pairs(VARIABLES) do
    if(message.type == "sensor_update_"..variable) then
      local outer = value_store[current_minute]
      local next_slot = #outer[variable] + 1
      value_store[current_minute][variable][next_slot] = message.value
    end
  end

  if(message.type == "trigger_calculate_average") then
    for minute, data in pairs(value_store) do
      print("- "..minute.." - "..current_minute)
      if(minute < current_minute) then
        print("is smaller")

        local publication = Publication.new("type", "node_environment_info", "timestamp", minute*60)
        local should_publish = false

        for variable, _configuration in pairs(VARIABLES) do
          if(#data[variable] > 0) then
            should_publish = true
            local current_sum = 0
            for i = 1,#data[variable] do
              item = data[variable][i]
              current_sum = current_sum + item
            end
            publication["value_"..variable] = current_sum / #data[variable]
            publication["num_"..variable] = #data[variable]
          end
        end
        value_store[minute] = nil
        
        if(should_publish) then
          print("publish"..minute)
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