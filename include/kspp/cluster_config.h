#include <string>
#include <chrono>
#pragma once

namespace kspp {
  class cluster_config {
  public:
    cluster_config();

    void load_config_from_env();

    void set_brokers(std::string uri);
    std::string get_brokers() const;

    void set_consumer_buffering_time(std::chrono::milliseconds timeout);
    std::chrono::milliseconds get_consumer_buffering_time() const;

    void set_producer_buffering_time(std::chrono::milliseconds timeout);
    std::chrono::milliseconds get_producer_buffering_time() const;

    void set_ca_cert_path(std::string path);
    std::string get_ca_cert_path() const;

    void set_private_key_path(std::string cert_path, std::string key_path, std::string passprase="");
    std::string get_client_cert_path() const;
    std::string get_private_key_path() const;
    std::string get_private_key_passphrase() const;

    void set_schema_registry(std::string);
    std::string get_schema_registry() const;

    void set_schema_registry_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds get_schema_registry_timeout() const;

    void set_storage_root(std::string s);
    std::string get_storage_root() const;

    void set_fail_fast(bool state);
    bool get_fail_fast() const;

    void validate() const;

    void log() const;

  private:
    std::string brokers_;
    std::string ca_cert_path_;
    std::string client_cert_path_;
    std::string private_key_path_;
    std::string private_key_passphrase_;
    std::chrono::milliseconds producer_buffering_;
    std::chrono::milliseconds consumer_buffering_;
    std::chrono::milliseconds schema_registry_timeout_;
    std::string root_path_;
    std::string schema_registry_uri_;
    bool _fail_fast;
  };
}