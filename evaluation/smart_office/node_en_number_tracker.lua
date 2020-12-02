function receive(message)

  local current_seconds, current_nanos = unix_timestamp()
  local current_minute = current_seconds // 60

  if(not seen) then
    seen = {}
  end

  if(not seen[current_minute]) then
    seen[current_minute] = {}
  end
  
  if(message.type == "init") then
    sub = {type="ble_discovery"}
    sub["0x03"] = encode_base64(string.char(0x6F, 0xFD))
    subscribe(sub)

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

  if(message.type == "ble_discovery" and message['0x03'] == encode_base64(string.char(0x6F, 0xFD))) then
    decoded = decode_base64(message["0x16"])

    if(#decoded < 22) then
      print("Invalid EN Beacon")
      print(#decoded)
      return
    end

    key=""
    for i = 3, 3+15, 1 do
      key = key..string.format("%02x", string.byte(decoded, i))
    end

    seen[current_minute][key] = true
  end

  if(message.type == "trigger_calculate_average") then
    for minute, data in pairs(seen) do
      if(minute < current_minute) then

        local count = 0
        for key, _u in pairs(data) do
          count = count + 1
        end
        publish(Publication.new(
          "type", "node_en_key_count",
          "timestamp", minute*60,
          "num_keys",  count
        ))
        seen[minute] = nil
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