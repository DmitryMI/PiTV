#pragma once

#include <vector>
#include <string>
#include <spdlog/spdlog.h>
#include <map>
#include <mongoose.h>
#include "video/Pipeline.h"
#include "accounts/UserDb.h"


struct PiTvServerConfig
{
    std::shared_ptr<spdlog::logger> logger_ptr;

    std::string tls_ca_path;
    std::string tls_pub_path;
    std::string tls_key_path;

    std::vector<std::string> http_listeners;
    std::vector<std::string> https_listeners;

    std::string pitv_mount_point = "/camera";
    std::string recording_path;

    std::string user_db;
    int user_max_leases = 1;
};

struct LeaseEntry
{
    std::string guid;
    std::string user;
    std::string udp_host;
    int udp_port;
    uint64_t lease_end_time;
};

struct PiTvServerStatus
{
    bool temperature_cpu_ok = false;
    double temperature_cpu = 0;

    bool load_cpu_process_ok = false;
    double load_cpu_process = 0;

    bool load_cpu_total_ok = false;
    double load_cpu_total = 0;
};

struct PiTvUser
{
    std::string username;
    std::map<std::string, LeaseEntry> lease_map;
};

class PiTvServer
{
private:
    bool is_server_running = false;
    bool system_stats_ok = false;
    bool system_stats_cpu_temp_ok = false;
    std::string tls_ca_value;
    std::string tls_cert_value;
    std::string tls_key_value;

    mg_mgr mongoose_event_manager;
    PiTvServerConfig config;
    std::string mongoose_log_buffer = "";
    spdlog::level::level_enum log_level;

    std::shared_ptr<Pipeline> pipeline_main_ptr;

    std::shared_ptr<UserDb> user_db;
    std::map<std::string, PiTvUser> user_map;

    std::string get_auth_username(mg_http_message* hm) const;

    static void mongoose_log_handler(char ch, void* param);
    static void server_http_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);
    static void server_https_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);

    static void timer_fn(void* data);

    void on_index_request(mg_connection* c, mg_http_message* hm);
    void on_pitv_request(mg_connection* c, mg_http_message* hm);
    void on_status_request(mg_connection* c, mg_http_message* hm) const;

    static std::string addr_to_str(const mg_addr& addr);

    static std::string gen_random_string(const int len);

    const char* get_tls_ca() const;
    const char* get_tls_cert() const;
    const char* get_tls_key() const;

    bool read_file(const std::string& file_name, std::string& out) const;

public:
    const static int guid_length;
    static const uint64_t max_lease_time_msec;

    PiTvServer& operator=(const PiTvServer&) = delete;
    PiTvServer(const PiTvServer& copy) = delete;
    PiTvServer() = delete;

    PiTvServer(const PiTvServerConfig& config, std::shared_ptr<Pipeline> pipeline);
    ~PiTvServer();
    bool start_server();
    bool server_poll(int timeout_msec);

    std::pair<int, std::string> lease_camera(std::string& guid, std::string username, std::string host, int port, uint64_t lease_time_msec);
    std::pair<int, std::string> end_camera_lease(std::string username, std::string guid);

    PiTvServerStatus get_server_status() const;
};