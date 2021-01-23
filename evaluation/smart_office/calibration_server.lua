CALIBRATION_DATA = {
  C7ACD4={example=-5.0 }
}

DEFAULT_CALIBRATION_DATA = {
  scd30_temperature = -4.75,
  bme280_temperature = -3.00
}

function receive(message)
  
  if(message.type == "init") then
    print("init calibration")
    subscribe({command="fetch_calibration_data"})
  end

  if(message.command == "fetch_calibration_data") then
    print("called fetch")
    calibration_value = nil
    if (CALIBRATION_DATA[message.c_node_id] and CALIBRATION_DATA[message.c_node_id][message.sensor_type]) then
      calibration_value = CALIBRATION_DATA[message.c_node_id][message.sensor_type]
    elseif(DEFAULT_CALIBRATION_DATA[message.sensor_type]) then
      calibration_value = DEFAULT_CALIBRATION_DATA[message.sensor_type] 
    end

    if(calibration_value) then
      publish(Publication.new(
        "node_id", message.publisher_node_id,
        "actor_type",message.publisher_actor_type,
        "instance_id", message.publisher_instance_id,
        "type", "sensor_calibration_data",
        "sensor_type", message.sensor_type,
        "value", calibration_value
      ))
    end
  end

end