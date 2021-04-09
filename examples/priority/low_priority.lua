function receive(message)

  if(message.type == "init") then
    subscribe({foo="bar"}, 1)
    delayed_publish(Publication.new("foo", "bar", "key_2", "baz"), 10000)
  end

  if(message.foo == "bar") then
    print("called 1")
  end

end