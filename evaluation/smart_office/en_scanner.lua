-- partially inspired by
-- https://github.com/Lurkars/esp-ena/blob/main/components/ena/ena-bluetooth-scan.c
function receive(message)
  
  if(message.type == "init") then
    sub = {type="ble_discovery"}
    sub["0x03"] = encode_base64(string.char(0x6F, 0xFD))
    subscribe(sub)
    --"0x03"="b/0="
    found = {}
  end

  if(message.type == "ble_discovery" and message['0x03'] == encode_base64(string.char(0x6F, 0xFD))) then
    decoded = decode_base64(message["0x16"])

    if(#decoded < 22) then
      print("Invalid EN Beacon")
      print(#decoded)
      return
    end

    key =""
    for i = 3, 3+15, 1 do
      key = key..string.format("%02x", string.byte(decoded, i))
    end

    meta =""
    for i = 19, 22, 1 do
      meta = meta..string.format("%02x", string.byte(decoded, i))
    end

    address_raw = decode_base64(message["address"])
    address = ""
    for i = 1, 6, 1 do
      address = address..string.format("%02x", string.byte(address_raw, i))
    end

    address_type = message["address_type"]

    if not found[key] then

      print(address_type.." - "..address.." - "..key.." - "..meta.." - "..message.rssi)

    -- io.write("\n")
    -- print().." - "..now())
      found[key] = now()
    end
  end
end

function dump(o)
    if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
          if type(k) ~= 'number' then k = '"'..k..'"' end
          s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
    else
      return tostring(o)
    end
end