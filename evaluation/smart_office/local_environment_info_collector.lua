VARIABLES = {
  temperature={unit="degree_celsius"},
  co2={unit="ppm"},
  relative_humidity={unit="percent"}
}

function receive(message)
  
  if(message.type == "init") then
    value_store = {}
    
    for variable, configuration in pairs(VARIABLES) do
      subscribe{type="sensor_update_"..variable, unit=configuration.unit, publisher_node_id=node_id}
      value_store[variable] = {}
    end

    current_seconds, current_nanos = unix_timestamp()

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
      local next_slot = #value_store[variable] + 1
      value_store[variable][next_slot] = message.value
    end
  end

  if(message.type == "trigger_calculate_average") then
    local publication = Publication.new("type", "node_environment_info")
    local should_publish = false

    for variable, _configuration in pairs(VARIABLES) do
      if(#value_store[variable] > 0) then
        should_publish = true
        local current_sum = 0
        for i = 1,#value_store[variable] do
          item = value_store[variable][i]
          current_sum = current_sum + item
        end
        publication["value_"..variable] = current_sum / #value_store[variable]
        publication["num_"..variable] = #value_store[variable]
  
        value_store[variable] = {}
      end
    end
    
    if(should_publish) then
      publish(publication)
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