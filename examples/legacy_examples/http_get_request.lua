function receive(message)
  if(message.type == "init") then
    subscribe({type="http_response", request_id=0})
    publish({node_id=node_id, type="http_request", http_method="GET",request_id=0, request_url="http://google.com"})
    print("Published Request")
  elseif(message.type == "http_response") then
    print(message.body)
  end
end
