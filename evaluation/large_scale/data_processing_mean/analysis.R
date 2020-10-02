library(ggplot2)
library(ggbeeswarm)
library(data.table)
library(magrittr)
library(tidyr)
library(readxl)
library(dplyr)

library(purrr)
library(stringr)

distance_to_center <- function(node_id) {
    node_id <- as.numeric(node_id)
    if(is.na(node_id)) {
      return(0);
    }
    current_sum = 0;
    for (height in 0:1000) {
      current_sum <- current_sum + 4**height;
      if(node_id <= current_sum) {
        return(height);
      }
    }
}

experiment_data <- function(experiment_file, experiment_name) {
  data <- fread(paste(experiment_file, ".csv", sep=""))
  data[,"experiment" := experiment_name]
  #data[,c("node", "variable", "experiment") := list(as.numeric(str_extract(variable, "\\d+")), str_match(variable, "^(.+[^_][^\\d]+)(_node_\\d+|$)")[,2], experiment_name)]
  #data[, distance := sapply(node, distance_to_center)]
  data <- data[, c("sequence_number", "value_s", "value_d", "group_index", "value_type") := NULL]
}

data <- experiment_data("time_series1", "time_series")
#data <- rbind(data, experiment_data("full2", "1_2"))
#data <- rbind(data, experiment_data("two_stage" ,"2_1"))
#data <- rbind(data, experiment_data("three_stage" ,"3_1"))
#data <- rbind(data, experiment_data("three_stage2" ,"3_2"))
#data <- rbind(data, experiment_data("three_stage2" ,"3_3"))
#data <- rbind(data, experiment_data("three_stage2" ,"3_4"))
#data <- rbind(data, experiment_data("four_stage", "4_1"))
#data <- rbind(data, experiment_data("five_stage", "5_1"))
#data <- rbind(data, experiment_data("five_stage2", "5_2"))
#data <- rbind(data, experiment_data("five_stage3", "5_3"))
#data <- rbind(data, experiment_data("five_stage4", "5_4"))
#data <- rbind(data, experiment_data("offload_two_stage", "o2_1"))
#data <- rbind(data, experiment_data("node_only", "n_1"))
# data <- rbind(data, experiment_data("offload_two_stage_loop", "9_offload_ts_loop"))

#data <- experiment_data("compare1", "single stage collector")
#data <- rbind(data, experiment_data("compare2", "two stage collector"))
#data <- rbind(data, experiment_data("compare3", "three stage collector"))

message_counts <- as.data.table(data[variable == "current_accepted_message_count"] %>% group_by(node, experiment) %>% summarise(value_i = sum(value_i))) # | variable == "rejected_message_count"]
message_counts[,distance := sapply(str_extract(node, "\\d+"), distance_to_center)]
message_size <- as.data.table(data[variable == "current_accepted_message_size"]  %>% group_by(node, experiment) %>% summarise(value_i = sum(value_i))) # | variable == "rejected_message_count"]
message_size[,distance := sapply(str_extract(node, "\\d+"), distance_to_center)]
processing_delay = data[variable == "processing_delay" & node == "node_1"]
num_received = data[variable == "num"] %>% group_by(experiment) %>% summarise(total_received = sum(value_i))
num_sent = data[variable == "total_sent"] %>% group_by(experiment) %>% summarise(total_received = sum(value_i))
#data <- data[variable == "num_messages_accepted" | variable == "receive" | variable == "time" | variable == "total"  | variable == "latency"]

# factor(sizes, levels = c("single stage collector", "two stage collector", "three stage collector", "four stage collector", "two stage offload"))

ggplot(processing_delay, aes(x=factor(experiment), y=value_i)) + geom_boxplot() +scale_y_continuous(name="processing delay")
#ggplot(processing_delay, aes(x=factor(experiment, levels = c("single stage collector", "two stage collector", "three stage collector", "four stage collector", "two stage offload", "node offload")), y=value_i)) + geom_boxplot() +scale_y_continuous(name="processing delay", labels=function(x)x/1000000)

# ggplot(message_counts, aes(x=distance, y=value_i, group=factor(experiment), shape = factor(variable))) + geom_boxplot(position = position_dodge2(preserve = "single")) +scale_y_continuous(name="number of message")

# ggplot(message_size, aes(distance, value_i, fill=factor(experiment))) + geom_bar(position = "dodge2", stat = "identity") +scale_y_continuous(name="message size (kB)", labels=function(x)x/1000)

ggplot(message_size, aes(experiment, value_i, fill=factor(distance))) + geom_bar(stat="identity") +scale_y_continuous(name="message size (MB)", labels=function(x)x/1000000)

ggplot(message_counts, aes(experiment, value_i, fill=factor(distance))) + geom_bar(stat="identity") +scale_y_continuous(name ="number of messages")


#ggplot(message_size, aes(x=node, y=value_i, color=factor(experiment), shape = factor(variable))) + geom_point(size=0.5) +scale_y_continuous(name="message size (mB)", labels=function(x)x/1000000)
    #, labels=function(x)x/1000,)
         #scale_x_discrete(name="concurrent messages") +
         #scale_fill_brewer(palette="Set1") +
         #theme_minimal())
  # )

