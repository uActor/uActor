PREFIX = "/example_app"
PF_PREDICATE = 7

function receive(message)
  if(message.type == "init") then
    subscribe({type="ingress_http_request", http_url={PF_PREDICATE, PREFIX}, http_method="get"})
    requests = 0
  elseif(message.type == "ingress_http_request" and message.http_url == PREFIX.."/requests.json") then
    requests = requests + 1
    http_reply(message, Publication.new(    
      "http_headers", "Content-Type",
      "Content-Type", "application/json",
      "http_body", "{\"request\": "..requests.."}"
    ))
  elseif(message.type == "ingress_http_request" and string.find(message.http_url, PREFIX) == 1 and RESOURCES[string.sub(message.http_url, #PREFIX+2)] ~= nil) then
      local item = RESOURCES[string.sub(message.http_url, #PREFIX+2)]
      http_reply(message, Publication.new(
          "http_headers", "Content-Type",
          "Content-Type", item.mime,
          "http_body", decode_base64(item.content)
        )) 
  end
end

function http_reply(message, reply)
  reply["type"] = "ingress_http_response"
  reply["http_request_id"] = message.http_request_id
  reply["node_id"] = message.publisher_node_id
  reply["actor_type"] = message.publisher_actor_type
  reply["instance_id"] = message.publisher_instance_id
  publish(reply)
end

--include <resources_generated>