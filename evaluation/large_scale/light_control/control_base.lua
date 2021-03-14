function receive(message)

  location_utils_receive_hook(message, location_info_ready_hook)

  if(message.type == "init") then
    light_state = {}
  end

  if(message.event == "light_control_button" or message.event == "light_control_motion_sensor") then
    local room = room_state(message)

    local desired_timeout = 0
    if((message.event == "light_control_button" and message.room == "hallway") or message.event == "light_control_motion_sensor") then
      desired_timeout = now()+30000 
    end
    if(not room["timeout"] or room["timeout"] ~= 0) then
      room["timeout"] = desired_timeout
      if(desired_timeout > 0) then
        publish_room_shutdown_wakeup(message.floor, message.wing, message.room)
      end
    end

    local current_state = 0
    if(room["desired_state"]) then
      current_state = room["desired_state"]
    end
    local desired_state = 0
    if(message.command == "on") then
      desired_state = 1
    end
    if(message.command == "switch" and current_state == 0) then
      desired_state = 1
    end
    room["desired_state"] = desired_state
    if(desired_state ~= current_state) then
      publish_room_command(room, message.floor, message.wing, message.room, message.time_sec, message.time_nsec)
    end
  end

  if(message.event == "light_control_lightbulb_added") then
    print("control bulb added")

    room = room_state(message)

    if(room) then
      publish_room_command(room, message.floor, message.wing, message.room)
    end
  end

  if(message.type == "check_light_timeout") then

    print("check")
    room = room_state(message)

    if(room and room["desired_state"] == 1) then
      timeout = room["timeout"] 
      if(timeout > 0 and timeout < now()) then
        print("check works")
        room.desired_state = 0
        room.timeout = 0
        publish_room_command(room, message.floor, message.wing, message.room)
      end
    end
  end
end

function location_info_ready_hook()
  local button_sub = {event="light_control_button"}
  local motion_sensor_sub = {event="light_control_motion_sensor"}
  local lightbulb_added_sub = {event="light_control_lightbulb_added"}

  if (CONTROL_DOMAIN == "building" or CONTROL_DOMAIN == "wing" or CONTROL_DOMAIN == "room") then
    button_sub["building"] = location_labels["building"]
    motion_sensor_sub["building"] = location_labels["building"]
    lightbulb_added_sub["building"] = location_labels["building"]
  end

  if (CONTROL_DOMAIN == "wing" or CONTROL_DOMAIN == "room") then
    button_sub["floor"] = location_labels["floor"]
    motion_sensor_sub["floor"] = location_labels["floor"]
    lightbulb_added_sub["floor"] = location_labels["floor"]

    button_sub["wing"] = location_labels["wing"]
    motion_sensor_sub["wing"] = location_labels["wing"]
    lightbulb_added_sub["wing"] = location_labels["wing"]
  end

  if (CONTROL_DOMAIN == "room") then
    button_sub["room"] = location_labels["room"]
    motion_sensor_sub["room"] = location_labels["room"]
    lightbulb_added_sub["room"] = location_labels["room"]
  end

  subscribe(button_sub)
  subscribe(motion_sensor_sub)
  subscribe(lightbulb_added_sub)
end

function room_state(message)
  local current = light_state
  for _id, key in ipairs(LOCATION_LABEL_KEYS) do
    if(key ~= "building") then
      if(not current[message[key]]) then
        current[message[key]] = {}
      end
      current = current[message[key]]
    end
  end
  return current
end

function publish_room_command(room_state, floor, wing, room, time_sec, time_nsec)
  local command = "off"
  if(room_state["desired_state"]==1) then
    command = "on"
  end
  publish(
    Publication.new(
      "type", "light_control_command",
      "building", location_labels["building"],
      "floor", floor,
      "wing", wing,
      "room", room,
      "command", command,
      "time_sec", time_sec,
      "time_nsec", time_nsec
    )
  )
end

function publish_room_shutdown_wakeup(floor, wing, room)
  delayed_publish(
    Publication.new(
      "node_id", node_id,
      "actor_type", actor_type,
      "instance_id", instance_id,
      "type", "check_light_timeout",
      "floor", floor,
      "wing", wing,
      "room", room
    ),
    30050
  )
end

--include <location_utils>