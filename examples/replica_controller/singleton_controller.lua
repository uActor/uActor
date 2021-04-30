DEPLOYMENT_NAME = "EXAMPLE_DEPLOYMENT"

SINGLETON_ID = "singleton."..DEPLOYMENT_NAME

--- internal configuration

ELECTION_PERIOD = 10000
HEALTH_CHECK_PERIOD = 5000
FAILED_HEALTH_CHECK_PERIODS = 3
STATES= {INIT=1, LEADER_WAITING=2, FOLLOWER_WAITING=3, LEADER=4, FOLLOWER=5, INACTIVE_FOLLOWER=6, MIGRATION_WAITING=7}

function receive(message)
  if(message.type == "init") then

    entries_received = 0

    deployment_running = false

    print("Init Singleton Controller")

    subscribe({
      type="singleton_election_current_leader",
      singleton_id=SINGLETON_ID,
      publisher_node_id={2, node_id, optional=true}
    })

    subscribe({
      type="singleton_election_node_entry",
      singleton_id=SINGLETON_ID,
      publisher_node_id={2, node_id, optional=true},
      processor_node_id={2, node_id, optional=true} 
    }, 1)

    start_new_vote()
  end

  if(message.type == "singleton_election_node_entry") then
    entries_received = entries_received + 1
    print("Receive Entry "..entries_received)

    
    local larger_than_current_leader = (message.score > current_leader.score or (message.score == current_leader.score and message.node_id > current_leader.node_id))

    if(larger_than_current_leader) then
      
      print("Larger entry received - Update leader structure")
      current_leader = {
        node_id=message.node_id,
        score=message.score,
        elected_at=now()
      }
      last_leader_health_beacon = now()
      
      if(current_state == STATES.LEADER_WAITING) then
        current_state = STATES.FOLLOWER_WAITING
        enqueue_wakeup(ELECTION_PERIOD + 100, "last_vote_casted_timeout")
      elseif (current_state == STATES.FOLLOWER_WAITING or current_state == STATES.MIGRATION_WAITING) then
        enqueue_wakeup(ELECTION_PERIOD + 100, "last_vote_casted_timeout")
      elseif (current_state == STATES.FOLLOWER or current_state == STATES.INACTIVE_FOLLOWER) then
        leader_elected_hook(true)
      elseif (current_state == STATES.LEADER) then
        lost_leadership_hook()
        -- while we could switch to the new service immediately, this could lead to multiple
        -- leadership changes when new nodes are added to an existing deployment
        current_state = STATES.MIGRATION_WAITING
        enqueue_wakeup(ELECTION_PERIOD + 100, "last_vote_casted_timeout")
      end
      republish(message)
    elseif(message.node_id < current_leader.node_id) then
      print(message.node_id.." smaller "..current_leader.node_id)
      if(current_state == STATES.LEADER or current_state == STATES.FOLLOWER) then
        print("send current leader")
        send_current_leader(SINGLETON_ID)
      elseif(message.node_id < node_id) then
        print("send own node information")
        -- these are not needed in most situations but might provide benefits if there are lost messages
        send_local_data(SINGLETON_ID)
      else
        republish(message)
      end
    end
  end

  if(message.type == "singleton_election_current_leader") then
    print("Receive Leader Message")
    if(message_has_higher_score(message)) then
      current_leader = {
        node_id=message.leader_id,
        score=message.score,
        elected_at=now()
      }
      last_leader_health_beacon = now()
      if(current_state == STATES.LEADER) then
        lost_leadership_hook()
        current_state = STATES.MIGRATION_WAITING
        enqueue_wakeup(ELECTION_PERIOD + 100, "last_vote_casted_timeout")
      else
        leader_elected_hook()
      end
      current_state = STATES.FOLLOWER
    elseif((current_state == STATES.FOLLOWER or current_state == STATES.INACTIVE_FOLLOWER) and message.publisher_node_id == current_leader.node_id) then
      last_leader_health_beacon = now()
      current_state = STATES.FOLLOWER
    end 
  end

  if(message.type == "wakeup" and message.wakeup_id == "last_vote_casted_timeout") then
    print("Receive VoteTimeout")
    if(current_leader.elected_at + HEALTH_CHECK_PERIOD < now()) then
      if(current_state == STATES.FOLLOWER_WAITING or current_state == STATES.MIGRATION_WAITING) then
        leader_elected_hook()
        current_state = STATES.FOLLOWER
        enqueue_wakeup(HEALTH_CHECK_PERIOD, "health_check")
      elseif(current_state == STATES.LEADER_WAITING) then
        elected_as_leader_hook()
        current_state = STATES.LEADER
        enqueue_wakeup(HEALTH_CHECK_PERIOD, "health_check")
      end
    end
  end

  if(message.type == "wakeup" and message.wakeup_id == "health_check") then
    print("health_check")
    local leader_inactive = (last_leader_health_beacon + HEALTH_CHECK_PERIOD*FAILED_HEALTH_CHECK_PERIODS) < now()
    if(current_state == STATES.FOLLOWER and leader_inactive) then
      print("became inactive")
      current_state = STATES.INACTIVE_FOLLOWER
    elseif(current_state == STATES.INACTIVE_FOLLOWER and leader_inactive) then
      start_new_vote()
    elseif(current_state == STATES.LEADER) then
      send_current_leader(SINGLETON_ID)
    end
    enqueue_wakeup(HEALTH_CHECK_PERIOD, "health_check")
  end
end

function start_new_vote()
  print("start new vote")
  send_local_data(SINGLETON_ID)
  current_leader = {node_id=node_id, score=node_id, elected_at=now()}
  last_leader_health_beacon = now()
  current_state = STATES.LEADER_WAITING
  enqueue_wakeup(ELECTION_PERIOD + 100, "last_vote_casted_timeout")
end

function send_local_data(singleton_election_id)
  delayed_publish(Publication.new(
    "type", "singleton_election_node_entry",
    "singleton_id", singleton_election_id,
    "node_id", node_id,
    "score", node_score()
  ), 100)
end

function node_score()
  return node_id
end

function message_has_higher_score(message)
  return message.score > current_leader.score or (message.score == current_leader.score and message.leader_id > current_leader.node_id)
end

function send_current_leader(singleton_election_id)
  publish(Publication.new(
    "type", "singleton_election_current_leader",
    "singleton_id", singleton_election_id,
    "leader_id", current_leader.node_id,
    "score", current_leader.score
  ))
end

function lost_leadership_hook()
  print("Lost leadership (waiting "..ELECTION_PERIOD.."ms for better alternatives), might require migration")
  if(deployment_running) then
    cancel_deployment(DEPLOYMENT_NAME, true)
  end
  deployment_running = false
end

function elected_as_leader_hook()
  print("Elected as leader!")
  deploy(DEPLOYMENT_NAME, code_hash(DEPLOYMENT_CODE), 0, {}, {node_id=node_id}, DEPLOYMENT_CODE)
  deployment_running = true
end

function leader_elected_hook(update)
  update = update or false
  print("Node "..current_leader.node_id.." elected as leader")
end

DEPLOYMENT_CODE = [[
--include <example_deployment>
]]

--include <deployment_utils>