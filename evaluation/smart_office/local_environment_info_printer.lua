VARIABLES = {
  temperature={unit="degree_celsius", repeatability=0.1, name_printed="TEMP", unit_printed="°C", sensor="bme280", alt_sensor="scd30"},
  co2={unit="ppm", repeatability=10, name_printed="CO2", unit_printed="ppm", sensor="scd30"},
  relative_humidity={unit="percent", repeatability=0.1 , name_printed="RH", unit_printed="%", sensor="bme280", alt_sensor="scd30"},
  pressure={unit="pa", repeatability=0.1 , name_printed="Pressure", unit_printed="Pa", sensor="bme280"}
}

function receive(message)
  
  if(message.type == "init") then
    comparison_values = {}
    output_values = {}
    num_values = 0
    total_num_values = 0
    alt_sensor_sub_ids = {}

    for variable, configuration in pairs(VARIABLES) do
      total_num_values = total_num_values + 1
      subscribe{type="sensor_update_"..variable, unit=configuration.unit, publisher_node_id=node_id, sensor=configuration.sensor}
      if(configuration.alt_sensor) then
        alt_sensor_sub_ids[variable] = subscribe{type="sensor_update_"..variable, unit=configuration.unit, publisher_node_id=node_id, sensor=configuration.alt_sensor} 
      end
    end
  end


  for variable, configuration in pairs(VARIABLES) do
    if(message.type == "sensor_update_"..variable) then
      if(message.sensor == configuration.sensor and alt_sensor_sub_ids[variable]) then
        unsubscribe(alt_sensor_sub_ids[variable])
        alt_sensor_sub_ids[variable] = nil
      end
      handle_value(variable, message.value, configuration.repeatability)
    end
  end

  if(message.type == "exit") then
    Publication.new(
      "type", "notification_cancelation",
      "node_id", node_id,
      "notification_id", node_id.."."..actor_type.."."..instance_id..".environment_information"
    )
  end

end

function handle_value(variable, value, repeatability)
  output_values[variable] = value
  if(not comparison_values[variable]) then
    comparison_values[variable] = value
    num_values = num_values + 1
    update_notification()
  elseif(value > comparison_values[variable] + repeatability or value < comparison_values[variable] - repeatability) then
    comparison_values[variable] = value
    update_notification()
  end
end

function update_notification()
  local notification_text = ""
  
  for variable, configuration in pairs(VARIABLES) do
    if(output_values[variable]) then
      comparison_values[variable] = output_values[variable]
      notification_text = notification_text..configuration.name_printed..":\n"
      notification_text = notification_text..string.format("%.f", output_values[variable]).." "..configuration.unit_printed.."\n"
    end
  end

  publish(
    Publication.new(
      "type", "notification",
      "node_id", node_id,
      "notification_id", node_id.."."..actor_type.."."..instance_id..".environment_information",
      "notification_text", notification_text,
      "notification_lifetime", 0
    )
  )
end