function receive(message)
  
  if(message.type == "init") then
    subscribe{type="node_environment_info"}
  end

  if(message.type == "node_environment_info") then
    print(
      "Node: "..message.publisher_node_id..
      "\n-CO2 PPM: "..message.value_co2..
      "\n-TEMP deg. C.: "..message.value_temperature..
      "\n-RH %: "..message.value_relative_humidity
    )
  end
end