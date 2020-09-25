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
  data[,c("node", "variable", "experiment") := list(as.numeric(str_extract(variable, "\\d+")), str_match(variable, "^(.+[^_][^\\d]+)(_node_\\d+|$)")[,2], experiment_name)]
  data[, distance := sapply(node, distance_to_center)]
  data <- data[, c("sequence_number", "value_s", "value_d", "group_index", "value_type") := NULL]
}

data <- experiment_data("two_stage_test", "two stage collector")
data <- rbind(data, experiment_data("full_test", "single stage collector"))
data <- rbind(data, experiment_data("three_stage_test" ,"three stage collector"))
data <- rbind(data, experiment_data("four_stage_test", "four stage collector"))
data <- rbind(data, experiment_data("offload_two_stage_test", "two stage offload"))
data <- rbind(data, experiment_data("nodec_two_stage", "node offload"))

message_counts <- data[variable == "accepted_message_count"]# | variable == "rejected_message_count"]
message_size <- data[variable == "accepted_message_size"]# | variable == "rejected_message_size"]
processing_delay = data[variable == "processing_delay"]
ggplot(processing_delay, aes(x=factor(experiment, levels = c("single stage collector", "two stage collector", "three stage collector", "four stage collector", "two stage offload", "node offload")), y=value_i)) + geom_boxplot() +scale_y_continuous(name="processing delay")

ggplot(message_counts, aes(x=distance, y=value_i, group=factor(experiment), shape = factor(variable))) + geom_boxplot(position = position_dodge2(preserve = "single")) +scale_y_continuous(name="number of message")

ggplot(message_size, aes(experiment, value_i, fill=factor(distance))) + geom_bar(stat="identity") +scale_y_continuous(name="message size (MB)", labels=function(x)x/1000000)

ggplot(message_counts, aes(experiment, value_i, fill=factor(distance))) + geom_bar(stat="identity") +scale_y_continuous(labels=function(x)x/1000)
