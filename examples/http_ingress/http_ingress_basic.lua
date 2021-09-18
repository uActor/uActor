function receive(message)
  if(message.type == "init") then
    print("init")
    subscribe({type="ingress_http_request", http_url="/index.html", http_method="get"})
    requests = 0
  elseif(message.type == "ingress_http_request") then
    requests = requests + 1
    
    print(message.http_method)
    print(message.http_body)
    print(message.http_headers)
    print(message.http_url)
    print(message.http_request_id)

    publish(Publication.new(
      "node_id", message.publisher_node_id,
      "actor_type", message.publisher_actor_type,
      "instance_id", message.publisher_instance_id,
      "type", "ingress_http_response",
      "http_request_id", message.http_request_id,
      "http_headers", "Content-Type",
      "Content-Type", "text/html",
      "http_body", "<html><body><h1>Hello from "..node_id..": "..requests.."</h1></body></html>"
    ))
  end
end