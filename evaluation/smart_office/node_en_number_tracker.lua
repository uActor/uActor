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
    sub = {type="ble_discovery", publisher_node_id=node_id}
    sub["0x03"] = encode_base64(string.char(0x6F, 0xFD))
    subscribe(sub)

    enqueue_wakeup((60-current_seconds%60)*1000, "calculate_average")
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

  if(message.type == "wakeup" and message.wakeup_id ==  "calculate_average") then
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

    enqueue_wakeup(60000, "calculate_average")

  end
end