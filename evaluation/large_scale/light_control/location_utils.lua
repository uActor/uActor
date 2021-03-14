LOCATION_LABEL_KEYS = {"building", "floor", "wing", "room"}

function location_utils_receive_hook(message, ready_hook)
  if(message.type == "init") then
    location_labels = {}
    received_label_count = 0
    ready = false
    location_utils_fetch_labels(LOCATION_LABEL_KEYS)
  end

  if (message.type == "label_response") then
    if(not location_labels[message.key]) then
      location_labels[message.key] = message.value
      received_label_count = received_label_count + 1
      if(received_label_count == #LOCATION_LABEL_KEYS) then
        ready = true
        if(ready_hook) then
          ready_hook()
        end
      end
    end
  end
end

function location_utils_add_location(publication)
  for location_key, location_value in pairs(location_labels) do
    publication[location_key] = location_value
  end
end

function location_utils_fetch_labels(labels)
  for _index, label in pairs(labels) do
    publish(Publication.new(
        "type", "label_get",
        "node_id", node_id,
        "key", label
    ))
  end
end