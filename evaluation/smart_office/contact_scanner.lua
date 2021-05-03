function receive(message)
  
  if(message.type == "init") then
    -- print("SCANNER - Init")
    local sub = {type="ble_discovery", publisher_node_id=node_id}
    
    name = string.char(0x47, 0x69, 0x67, 0x61, 0x73, 0x65, 0x74, 0x20, 0x47, 0x2D, 0x74, 0x61, 0x67)
    sub["0x09"] = encode_base64(name)
    print("before subscribe")
    subscribe(sub)
    seen = {}
    contacts = {}

    enqueue_wakeup(30000, "cleanup_trigger")
  end

  if(message.type == "ble_discovery" and message["0x09"] == encode_base64(name) and message.rssi >= -70) then
    -- print("SCANNER - Message")
    local decoded_address = decode_base64(message.address)
    if not seen[decoded_address] then
      print("New keychain: "..to_hex(decoded_address))
      for address, last_seen in pairs(seen) do
        if(now() - last_seen < 30000 and address ~= decoded_address) then
          print("Contact start: " .. to_hex(decoded_address) .. " - " .. to_hex(address))
          contacts[address..decoded_address] = {peer_1=address, peer_2=decoded_address, begin=now()}
        else
          print("Insert: Erase "..to_hex(decoded_address))
          -- delete_key(address)
        end
      end
    end
    seen[decoded_address] = now()
  end

  if(message.type == "wakeup" and message.wakeup_id == "cleanup_trigger") then
    -- print("SCANNER - Wakeup")
    local keys_to_delete = {}
    
    for address, last_seen in pairs(seen) do
      if(now() - last_seen > 30000) then
        print("Cleanup: Erase "..to_hex(address))
        keys_to_delete[#keys_to_delete+1] = address
      end
    end

    for _id, key in ipairs(keys_to_delete) do
      delete_key(key)
    end

    for key, contact in pairs(contacts) do
      local duration_s = (now()-contacts[key]["begin"])/1000
      print("Contact duration " .. to_hex(contact["peer_1"]) .. " - " .. to_hex(contact["peer_2"])..":"..duration_s)
    end 

    enqueue_wakeup(30000, "cleanup_trigger")
  end

  if(message.type == "exit") then
    print("Exit")
    local to_delete = {}
    for address, _last_seen in pairs(seen) do
      to_delete[#to_delete+1] = address
    end
    for _key, address in ipairs(to_delete) do
      print("Erase: "..to_hex(address))
      delete_key(address)
    end
  end
end

function delete_key(address)
  local contacts_to_delete = {}
  
  for key, contact in pairs(contacts) do
    if(contact["peer_1"] == address or contact["peer_2"] == address) then
      contacts_to_delete[#contacts_to_delete+1] = key
    end
  end
  
  for _id, key in ipairs(contacts_to_delete) do
    local contact = contacts[key]
    local duration_s = (now()-contacts[key]["begin"])/1000
    print("Contact end: " .. to_hex(contact["peer_1"]) .. " - " .. to_hex(contact["peer_2"]).. "; Duration: ".. duration_s)
    contact_publication = Publication.new(
      "type", "contact_tracing.contact",
      "peer_1", encode_base64(contact["peer_1"]),
      "peer_2", encode_base64(contact["peer_2"]),
      "duration", duration_s,
      "end_timestamp", unix_timestamp(),
      "reported_by", node_id
    )
    contacts[key] = nil
    contact = nil
  end
  seen[address] = nil
end

function to_hex(binary)
  if (not binary) then
    return ""
  end
  local hex = ""
  for i = 1, #binary, 1 do
    hex = hex..string.format("%02x", string.byte(binary, i))
  end
  return hex
end