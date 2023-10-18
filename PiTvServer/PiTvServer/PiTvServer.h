#pragma once

#include <vector>
#include <string>
#include <spdlog/spdlog.h>
#include <map>
#include <mongoose.h>
#include "video/Pipeline.h"


struct PiTvServerConfig
{
    std::shared_ptr<spdlog::logger> logger_ptr;

    std::string tls_ca_str;
    std::string tls_pub_str;
    std::string tls_priv_str;

    std::vector<std::string> http_listeners;
    std::vector<std::string> https_listeners;

    std::string pitv_mount_point = "/camera";
    std::string recording_path;

    int user_max_leases = 2;
};

struct LeaseEntry
{
    std::string guid;
    std::string user;
    std::string udp_host;
    int udp_port;
    uint64_t lease_end_time;
};

struct PiTvUser
{
    std::string username;
    std::string password;
    std::map<std::string, LeaseEntry> lease_map;
};

class PiTvServer
{
private:
    bool is_server_running = false;

    mg_mgr mongoose_event_manager;
    PiTvServerConfig config;
    std::string mongoose_log_buffer = "";
    spdlog::level::level_enum log_level;

    std::shared_ptr<Pipeline> pipeline_main_ptr;

    std::map<std::string, PiTvUser> user_map;

    std::string get_auth_username(mg_http_message* hm) const;

    static void mongoose_log_handler(char ch, void* param);
    static void server_http_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);
    static void server_https_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);

    static void timer_fn(void* data);

    void on_index_request(mg_connection* c, mg_http_message* hm);
    void on_pitv_request(mg_connection* c, mg_http_message* hm);

    static std::string addr_to_str(const mg_addr& addr);

    static std::string gen_random_string(const int len);

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
};