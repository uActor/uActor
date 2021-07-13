function receive(message)
  if(message.type == "init") then
    publish(Publication.new(
      "type", "http_request",
      "http_method", "GET",
      "request_id", 0,
      "request_url", "http://webhookinbox.com/"
      ))
    publish(Publication.new(
     "type", "http_request",
     "http_method", "GET",
     "request_id", 1,
     "request_url", "http://webhookinbox.com/",
     "headers", "Accept-Charset: utf-8;Accept-Language: en-US"))
  elseif(message.type == "http_response") then
    if (message.http_code == 200) then
      print(tostring(message.request_id)..  ": "  ..message.body)
    else 
      print(tostring(message.request_id).. " did not succeed http code: " ..tostring(message.http_code))
    end
  end
end
