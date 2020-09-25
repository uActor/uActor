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

data <- experiment_data("setup_overhead", "node first")
data <- rbind(data, experiment_data("setup_overhead_cloud_first", "root first"))
data <- rbind(data, experiment_data("setup_overhead_cloud_first_loop", "cloud first loop"))
data <- rbind(data, experiment_data("setup_overhead_loop", "root first loop"))

message_counts <- data[variable == "accepted_message_count"]# | variable == "rejected_message_count"]
message_size <- data[variable == "accepted_message_size"]# | variable == "rejected_message_size"]

ggplot(message_size, aes(experiment, value_i, fill=factor(distance))) + geom_bar(stat="identity") +scale_y_continuous(name="message size (MB)", labels=function(x)x/1000000)

ggplot(message_counts, aes(experiment, value_i, fill=factor(distance))) + geom_bar(stat="identity") +scale_y_continuous(labels=function(x)x/1000)
