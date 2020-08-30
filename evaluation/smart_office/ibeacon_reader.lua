function receive(message)
  
  if(message.type == "init") then
    sub = {type="ble_discovery"}
    sub["0xFF"] = {GT, "", optional=false}
    subscribe(sub)
  end

  if(message.type == "ble_discovery" and message["0xFF"]) then
    decoded = decode_base64(message["0xFF"])
    if(not #decoded == 25) then
      return
    end
    if(string.byte(decoded, 1) == 0x4c and string.byte(decoded, 2) == 0x00 and string.byte(decoded, 3) == 0x02 and string.byte(decoded, 4) == 0x15) then
      uuid = ""
      for i = 5, 8, 1 do
        uuid = uuid..string.format("%02x", string.byte(decoded, i))
      end
      uuid = uuid.."-" 
      for i = 9, 10, 1 do
        uuid = uuid..string.format("%02x", string.byte(decoded, i))
      end
      uuid = uuid.."-"
      for i = 11, 12, 1 do
        uuid = uuid..string.format("%02x", string.byte(decoded, i))
      end
      uuid = uuid.."-"
      for i = 13, 14, 1 do
        uuid = uuid..string.format("%02x", string.byte(decoded, i))
      end
      uuid = uuid.."-" 
      for i = 15, 20, 1 do
        uuid = uuid..string.format("%02x", string.byte(decoded, i))
      end

      major = string.format("0x%02x%02x", string.byte(decoded, 22), string.byte(decoded, 21))
      minor = string.format("0x%02x%02x", string.byte(decoded, 24), string.byte(decoded, 23))
      
      print("iBeacon: "..uuid.." - "..major.." - "..minor.." - "..string.byte(decoded, 25))
    end
  end
end