function receive(message)
    if(message.type == "init") then
      delayed_publish(Publication.new(
        "type", "notification",
        "node_id", node_id,
        "notification_text", "TestNotification30s",
        "notification_id", "test_notification_30s",
        "notification_lifetime", 30000
      ), 5000)
      delayed_publish(Publication.new(
        "type", "notification",
        "node_id", node_id,
        "notification_text", "TestNotification60s",
        "notification_id", "test_notification_60s",
        "notification_lifetime", 60000
      ), 5000)
      delayed_publish(Publication.new(
        "type", "notification",
        "node_id", node_id,
        "notification_text", "TestNotification Unlimited",
        "notification_id", "test_notification_unlimited",
        "notification_lifetime", 0
      ), 5000)
    end

    if(message.type == "exit") then
      publish(
        Publication.new(
          "type", "notification_cancelation",
          "node_id", node_id,
          "notification_id", "test_notification_unlimited"
        )
      )
    end
end