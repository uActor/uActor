function receive(message)
  if(message.type == "init") then
    subscribe({type="http_response", request_id=0})
    publish({node_id=node_id, type="http_request", http_method="POST",request_id=0, request_url="http://api.webhookinbox.com/i/pPyCZ7gI/in/", attributes="test1, test2 ,test3", test1="foo", test2="bar"})
    print("Published Request")
  elseif(message.type == "http_response") then
    print(message.code)
  end
end
