function receive(message)
  if(message.type == "init") then
    subscribe({type="http_response", request_id=0})
    publish(Publication.new(
      "type", "http_request",
      "http_method", "POST",
      "request_id", 0,
      "request_url", "http://api.webhookinbox.com/i/YH2wIV4M/in/",
      "attributes", "test1,test2,test3",
      "test1", "foo", 
      "test2", "bar"))
    publish(Publication.new(
      "type", "http_request",
      "http_method", "POST",
      "request_id", 0,
      "request_url", "http://api.webhookinbox.com/i/YH2wIV4M/in/",
      "headers", "Accept-Charset: utf-8|Accept-Language: en-US",
      "attributes", "test1,test2,test3",
      "test1", "foo", 
      "test2", "bar"))
  elseif(message.type == "http_response") then
    print(message.code)
  end
end
