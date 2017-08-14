#include <kspp/kspp.h>
#include <iostream>

using namespace std::chrono_literals;

namespace kspp {
  topology_base::topology_base(std::shared_ptr<app_info> ai,
                               std::string topology_id,
                               std::string brokers,
                               std::chrono::milliseconds max_buffering,
                               boost::filesystem::path root_path)
          : _app_info(ai), _is_init(false), _topology_id(topology_id), _brokers(brokers),
            _max_buffering_time(max_buffering), _root_path(root_path), _next_gc_ts(0) {
    LOG(INFO) << "topology created, name:" << name();
  }

  topology_base::~topology_base() {
    LOG(INFO) << "topology, name:" << name() << " terminating";
    _top_partition_processors.clear();
    _partition_processors.clear();
    _sinks.clear();
    LOG(INFO) << "topology, name:" << name() << " terminated";
  }

  std::string topology_base::app_id() const {
    return _app_info->identity();
  }

  std::string topology_base::group_id() const {
    return _app_info->group_id();
  }

  std::string topology_base::topology_id() const {
    return _topology_id;
  }

  std::string topology_base::brokers() const {
    return _brokers;
  }

  std::chrono::milliseconds topology_base::max_buffering_time() const {
    return _max_buffering_time;
  }

  std::string topology_base::name() const {
    return "[" + _app_info->identity() + "]" + _topology_id;
  }

//the metrics should look like this...
//cpu_load_short, host=server01, region=us-west value=0.64 1434055562000000000
//metric_name,app_id={app_id}},topology={{_topology_id}},depth={{depth}},processor_type={{processor_name()}},record_type="
//order tags descending
  static std::string escape_influx(std::string s) {
    std::string s2 = boost::replace_all_copy(s, " ", "\\ ");
    std::string s3 = boost::replace_all_copy(s2, ",", "\\,");
    std::string s4 = boost::replace_all_copy(s3, "=", "\\=");
    return s4;
  }

  void topology_base::init_metrics() {
    for (auto i : _partition_processors) {
      for (auto j : i->get_metrics()) {
        std::string metrics_tags = "depth=" + std::to_string(i->depth())
                                   + ",key_type=" + escape_influx(i->key_type_name())
                                   + ",partition=" + std::to_string(i->partition())
                                   + ",processor_type=" + escape_influx(i->simple_name())
                                   + ",record_type=" + escape_influx(i->record_type_name())
                                   + ",topology=" + escape_influx(_topology_id)
                                   + ",value_type=" + escape_influx(i->value_type_name());
        j->set_tags(metrics_tags);
      }
    }

    for (auto i : _sinks) {
      for (auto j : i->get_metrics()) {
        std::string metrics_tags = "key_type=" + escape_influx(i->key_type_name())
                                   + ",processor_type=" + escape_influx(i->simple_name())
                                   + ",record_type=" + escape_influx(i->record_type_name())
                                   + ",topology=" + escape_influx(_topology_id)
                                   + ",value_type=" + escape_influx(i->value_type_name());
        j->set_tags(metrics_tags);
      }
    }
  }

  void topology_base::for_each_metrics(std::function<void(kspp::metric &)> f) {
    for (auto i : _partition_processors)
      for (auto j : i->get_metrics())
        f(*j);

    for (auto i : _sinks)
      for (auto j : i->get_metrics())
        f(*j);
  }

  void topology_base::init() {
    _top_partition_processors.clear();

    for (auto i : _partition_processors) {
      bool upstream_of_something = false;
      for (auto j : _partition_processors) {
        if (j->is_upstream(i.get()))
          upstream_of_something = true;
      }
      if (!upstream_of_something) {
        LOG(INFO) << "topology << " << name() << ": adding " << i->simple_name() << " to top";
        _top_partition_processors.push_back(i);
      } else {
        LOG(INFO) << "topology << " << name() << ": skipping poll of " << i->simple_name();
      }
    }
    _is_init = true;
  }

  bool topology_base::eof() {
    for (auto &&i : _top_partition_processors) {
      if (!i->eof())
        return false;
    }
    return true;
  }

  int topology_base::process_one() {
    // this needs to be done to to trigger callbacks
    for (auto i : _sinks)
      i->poll(0);

    // some of those might be kafka_partition_sinks....
    for (auto i : _partition_processors)
      i->poll(0);

    // tbd partiotns sinks???
    // check sinks here an return 0 if we need to wait...
    // we should not check every loop
    // check every 1000 run?
    size_t sink_queue_len = 0;
    for (auto i : _sinks)
      sink_queue_len += i->queue_len();
    if (sink_queue_len > 50000)
      return 0;

    int64_t tick = milliseconds_since_epoch();

    int res = 0;
    for (auto &&i : _top_partition_processors) {
      res += i->process_one(tick);
    }

    for (auto i : _sinks)
      res += i->process_one(tick);

    if (tick > _next_gc_ts) {
      for (auto i : _partition_processors)
        i->garbage_collect(tick);
      for (auto i : _sinks)
        i->garbage_collect(tick);
      _next_gc_ts = tick + 10000; // 10 sec
    }

    return res;
  }

  void topology_base::close() {
    for (auto i : _partition_processors)
      i->close();
    for (auto i : _sinks)
      i->close();
  }

  void topology_base::start() {
    if (!_is_init)
      init();
    for (auto &&i : _top_partition_processors)
      i->start();
    //for (auto i : _topic_processors) // those are only sinks??
    //  i->start();
  }

  void topology_base::start(int offset) {
    if (!_is_init)
      init();
    for (auto &&i : _top_partition_processors)
      i->start(offset);
    //for (auto i : _topic_processors) // those are only sinks??
    //  i->start(offset);
  }

  void topology_base::commit(bool force) {
    if (!_is_init)
      init();
    for (auto &&i : _top_partition_processors)
      i->commit(force);
  }

// this is probably not enough if we have a topology that consists of 
// sources -> sink -> source -> processing > sink -> source
// we might have stuff in sinks that needs to be flushed before processing in following steps can finish
// TBD how to capture this??
// for now we start with a flush of the sinks but that is not enough
  void topology_base::flush() {
    while (true) {
      for (auto i : _sinks)
        i->flush();

      auto sz = process_one();
      if (sz)
        continue;
      if (sz == 0 && !eof())
        std::this_thread::sleep_for(10ms);
      else
        break;
    }

    for (auto i : _top_partition_processors)
      i->flush();

    while (true) {
      for (auto i : _sinks)
        i->flush();

      auto sz = process_one();
      if (sz)
        continue;
      if (sz == 0 && !eof())
        std::this_thread::sleep_for(10ms);
      else
        break;
    }
  }

  boost::filesystem::path topology_base::get_storage_path() {
    boost::filesystem::path top_of_topology(_root_path);
    top_of_topology /= sanitize_filename(_app_info->identity());
    top_of_topology /= sanitize_filename(_topology_id);
    DLOG(INFO) << "topology " << _app_info->identity() << ": creating local storage at " << top_of_topology;
    auto res = boost::filesystem::create_directories(top_of_topology);
    // seems to be a bug in boost - always return false...
    //if (!res)
    //  BOOST_LOG_TRIVIAL(error) << "topology " << _app_info->identity() << ": failed to create local storage at " << top_of_topology;
    // so we check if the directory exists after instead...
    if (!boost::filesystem::exists(top_of_topology))
      LOG(ERROR) << "topology " << _app_info->identity() << ": failed to create local storage at "
                               << top_of_topology;
    return top_of_topology;
  }

};
