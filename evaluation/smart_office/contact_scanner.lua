function receive(message)
  
  if(message.type == "init") then
    sub = {type="ble_discovery", publisher_node_id=node_id}
    -- "Gigaset G-tag" without null_termination
    name = string.char(0x47, 0x69, 0x67, 0x61, 0x73, 0x65, 0x74, 0x20, 0x47, 0x2D, 0x74, 0x61, 0x67)
    sub["0x09"] = encode_base64(name)
    subscribe(sub)
    seen = {}
    contacts = {}

    print(unix_timestamp())

    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "cleanup_trigger"), 5000)
  end

  if(message.type == "ble_discovery" and message["0x09"] == encode_base64(name) and message.rssi <= 70) then
    decoded_address = decode_base64(message.address)
    if not seen[decoded_address] then
      print("New Keychain: "..to_hex(decoded_address))
      for address, last_seen in pairs(seen) do
        if(now() - last_seen < 15000) then
          print("Contact start: " .. to_hex(decoded_address) .. " - " .. to_hex(address))
          contacts[address..decoded_address] = {peer_1=address, peer_2=decoded_address, begin=now()}
        end
      end
    else
      --- print(now() - seen[decoded_address])
    end
    seen[decoded_address] = now()
  end

  if(message.type == "cleanup_trigger") then
    local keys_to_delete = {}
    for address, last_seen in pairs(seen) do
      if(now() - last_seen > 15000) then
        print("Erase "..to_hex(decoded_address))
        keys_to_delete[#keys_to_delete+1] = address
        local contacts_to_delete = {}
        for key, contact in pairs(contacts) do
          if(contact["peer_1"] == address or contact["peer_2"] == address) then
            contacts_to_delete[#contacts_to_delete+1] = key
          end
        end
        for id, key in pairs(contacts_to_delete) do
          duration_s = (now()-contacts[key]["begin"])/1000
          print("Contact end: " .. to_hex(decoded_address) .. " - " .. to_hex(address).. "; Duration: ".. duration_s)
          contact_publication = Publication.new(
            "type", "contact_tracing.contact",
            "peer_1", encode_base64(decoded_address),
            "peer_2", encode_base64(address),
            "duration", duration_s,
            "end_timestamp", unix_timestamp(),
            "reported_by", node_id
          )
          contacts[key] = nil
        end
      end
      for id, key in pairs(keys_to_delete) do
        seen[key] = nil
      end
    end

    delayed_publish(Publication.new("node_id", node_id, "actor_type", actor_type, "instance_id", instance_id, "type", "cleanup_trigger"), 5000)
  end
end

function to_hex(binary)
  hex = ""
  for i = 1, #binary, 1 do
    hex = hex..string.format("%02x", string.byte(binary, i))
  end
  return hex
end