#include <vector>
#include <string>
#include <spdlog/spdlog.h>

#include <mongoose.h>


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
};

class PiTvServer
{
private:
    mg_mgr mongoose_event_manager;
    PiTvServerConfig config;
    std::string mongoose_log_buffer = "";
    spdlog::level::level_enum log_level;

    static void mongoose_log_handler(char ch, void* param);
    static void server_http_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);
    static void server_https_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);

    void on_index_get(mg_connection* c, mg_http_message* hm);
    void on_pitv_get(mg_connection* c, mg_http_message* hm);

public:
    PiTvServer& operator=(const PiTvServer&) = delete;
    PiTvServer(const PiTvServer& copy) = delete;
    PiTvServer() = delete;

    PiTvServer(const PiTvServerConfig& config);
    ~PiTvServer();
    bool start_server();
    bool server_poll(int timeout_msec);
};