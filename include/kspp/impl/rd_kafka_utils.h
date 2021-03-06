#include <memory>
#include <librdkafka/rdkafkacpp.h>
#include <kspp/cluster_config.h>
#pragma once

void set_config(RdKafka::Conf* conf, std::string key, std::string value);
void set_config(RdKafka::Conf* conf, std::string key, RdKafka::Conf* topic_conf);
void set_config(RdKafka::Conf* conf, std::string key, RdKafka::DeliveryReportCb* callback);
void set_config(RdKafka::Conf* conf, std::string key, RdKafka::PartitionerCb* partitioner_cb);
void set_broker_config(RdKafka::Conf* rd_conf, std::shared_ptr<kspp::cluster_config> cluster_config);

