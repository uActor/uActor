library(ggplot2)
library(ggbeeswarm)
library(data.table)
library(magrittr)
library(tidyr)
library(readxl)
library(dplyr)
require(bit64)

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

data <- fread("time_series1.csv")

data <- data[, c("value_s", "value_d", "group_index", "value_type") := NULL]
data <- data[variable == "current_queue_size_max" | variable == "current_accepted_message_count" | variable == "current_accepted_message_size" | variable == "current_rejected_message_count" |variable == "current_rejected_message_size" | variable == "current_message_information_timestamp" | variable == "current_sub_message_size" | variable == "current_deployment_message_size" | variable == "current_regular_message_size"]
data[variable == "current_accepted_message_count", sequence_number := sequence_number - 1]
data[variable == "current_accepted_message_size", sequence_number := sequence_number - 2]
data[variable == "current_rejected_message_count", sequence_number := sequence_number - 3]
data[variable == "current_rejected_message_size", sequence_number := sequence_number - 4]
data[variable == "current_sub_message_size", sequence_number := sequence_number - 5]
data[variable == "current_deployment_message_size", sequence_number := sequence_number - 6]
data[variable == "current_regular_message_size", sequence_number := sequence_number - 7]
data[variable == "current_queue_size_max", sequence_number := sequence_number - 8]
data_wide <- dcast(data, ... ~ variable, value.var = "value_i")


summarized_data = data_wide %>%
group_by(current_message_information_timestamp) %>%
summarise_at(c("current_queue_size_max", "current_accepted_message_count", "current_accepted_message_size", "current_rejected_message_count", "current_rejected_message_size", "current_sub_message_size", "current_deployment_message_size", "current_regular_message_size"), sum)

data_long <- melt(as.data.table(summarized_data), id.vars=c("current_message_information_timestamp"), variable.name = "component")

ggplot(data_long[component == "current_deployment_message_size" | component == "current_sub_message_size" | component == "current_regular_message_size"], aes(x=current_message_information_timestamp, y=value, fill=component)) + 
  geom_area() + theme_minimal()


ggplot(summarized_data, aes(x=current_message_information_timestamp)) +
  #geom_line(aes( y=current_queue_size_max)) +
  geom_line(aes(y=current_deployment_message_size), color = "green") +
  geom_line(aes(y=current_sub_message_size), color = "red") +
  geom_line(aes(y=current_regular_message_size), color = "yellow")# +
  #xlim(c(11013500, 11014200))

ggplot(summarized_data, aes(x=current_message_information_timestamp)) +
  geom_line(aes( y=current_queue_size_max)) #+
  #xlim(c(11013500, 11014200))
  #ylim(c(0, 2000000))
#+ scale_y_log10()
#+scale_y_continuous(name="kbyte/s", labels=function(x)x/1000)





