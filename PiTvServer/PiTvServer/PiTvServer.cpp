#include "PiTvServer.h"
#include "SystemStats.h"

const int PiTvServer::guid_length = 64;
const uint64_t PiTvServer::max_lease_time_msec = 60000;

void PiTvServer::timer_fn(void* data)
{
    PiTvServer* server = static_cast<PiTvServer*>(data);
    assert(server);

    uint64_t current_uptime = mg_millis();

    for (auto& user_entry : server->user_map)
    {
        std::vector<std::string> timeout_leases;
        for (auto& lease_entry : user_entry.second.lease_map)
        {
            if (lease_entry.second.lease_end_time <= current_uptime)
            {
                server->config.logger_ptr->info("Lease {} of user {} timeout", lease_entry.second.guid, lease_entry.second.user);
                timeout_leases.push_back(lease_entry.second.guid);
            }
        }

        for (auto lease_guid : timeout_leases)
        {
            server->end_camera_lease(user_entry.second.username, lease_guid);
        }
    }
}

void PiTvServer::server_http_handler(mg_connection* c, int ev, void* ev_data, void* fn_data)
{
    PiTvServer* server = static_cast<PiTvServer*>(fn_data);

    if (ev == MG_EV_HTTP_MSG)
    {
        mg_http_message* hm = (struct mg_http_message*)ev_data;
        if (mg_http_match_uri(hm, server->config.pitv_mount_point.c_str()))
        {
            server->on_pitv_request(c, hm);
        }
        else if (mg_http_match_uri(hm, "/index.html"))
        {
            server->on_index_request(c, hm);
        }
        else if (mg_http_match_uri(hm, "/status"))
        {
            server->on_status_request(c, hm);
        }
        else
        {
            struct mg_http_serve_opts opts = { .root_dir = server->config.recording_path.c_str()};
            mg_http_serve_dir(c, (mg_http_message*)ev_data, &opts);
        }
    }
}

void PiTvServer::mongoose_log_handler(char ch, void* param)
{
    PiTvServer* server = static_cast<PiTvServer*>(param);
    if (ch == '\n')
    {
        server->config.logger_ptr->log(server->log_level, server->mongoose_log_buffer);
        server->mongoose_log_buffer = "";
    }
    else
    {
        server->mongoose_log_buffer += ch;
    }
}

std::string PiTvServer::get_auth_username(mg_http_message* hm) const
{
    char username_c[256], pass_c[256];
    mg_http_creds(hm, username_c, sizeof(username_c), pass_c, sizeof(pass_c));
    
    std::string username(username_c);
    std::string password(pass_c);

    if (username.empty() || password.empty())
    {
        config.logger_ptr->error("Failed to get auth user from request: username or password is empty");
        return "";
    }

    if (!user_db)
    {
        config.logger_ptr->error("Failed to get auth user from request: user_db not constructed!");
        return "";
    }
    auto user_entry = user_db->get_userdata(username);
    if (!user_entry)
    {
        config.logger_ptr->error("Failed to get auth user from request: user {} not found!", username);
        return "";
    }

    if (user_entry->password != password)
    {
        config.logger_ptr->error("Failed to get auth user from request: wrong password.");
        return "";
    }

    return username;
}

void PiTvServer::on_status_request(mg_connection* c, mg_http_message* hm) const
{
    assert(hm);

    std::string method(hm->method.ptr, hm->method.len);
    config.logger_ptr->info("{} request on /status URI!", method);

    PiTvServerStatus status = get_server_status();

    mg_http_reply(c, 200, "Content-Type: application/json\r\n", 
        "{"
        "\"temp_cpu_ok\": %d,"
        "\"temp_cpu\": %3.2f,"
        "\"cpu_load_process_ok\": %d,"
        "\"cpu_load_process\": %3.2f,"
        "\"cpu_load_total_ok\": %d,"
        "\"cpu_load_total\": %3.2f"
        "}\n", 
        status.temperature_cpu_ok, status.temperature_cpu,
        status.load_cpu_process_ok, status.load_cpu_process,
        status.load_cpu_total_ok, status.load_cpu_total
    );
}

void PiTvServer::on_pitv_request(mg_connection* c, mg_http_message* hm)
{
    assert(hm);

    std::string method(hm->method.ptr, hm->method.len);

    config.logger_ptr->info("{} request on /camera URI!", method);

    std::string auth_user = get_auth_username(hm);
    if (auth_user.empty())
    {
        mg_http_reply(c, 401, "", "Unauthorized");
        return;
    }

    if (method != "POST")
    {
        config.logger_ptr->error("Unsupported method from {}", addr_to_str(c->rem));
        mg_http_reply(c, 405, "", "Unsupported method");
        return;
    }

    char* lease_guid = mg_json_get_str(hm->body, "$.lease_guid");
    if (!lease_guid)
    {
        config.logger_ptr->error("lease_guid field not found in request from {}", addr_to_str(c->rem));
        mg_http_reply(c, 400, "", "lease_guid field missing");
        return;
    }

    char* udp_address = mg_json_get_str(hm->body, "$.udp_address");
    if (!udp_address)
    {
        config.logger_ptr->error("udp_address field not found in request from {}", addr_to_str(c->rem));
        mg_http_reply(c, 400, "", "udp_address field missing");
        return;
    }

    long udp_port = mg_json_get_long(hm->body, "$.udp_port", -1);
    if (udp_port < 0)
    {
        config.logger_ptr->error("udp_port field not found or is malformed in request from {}", addr_to_str(c->rem));
        mg_http_reply(c, 400, "", "udp_port field missing");
        return;
    }

    long lease_time = mg_json_get_long(hm->body, "$.lease_time", -1);
    if (udp_port < 0)
    {
        config.logger_ptr->error("lease_time field not found or is malformed in request from {}", addr_to_str(c->rem));
        mg_http_reply(c, 400, "", "lease_time field missing");
        return;
    }

    std::string lease_guid_new = std::string(lease_guid);
    auto result = lease_camera(lease_guid_new, auth_user, std::string(udp_address), udp_port, lease_time);

    if (result.first == 200)
    {
        mg_http_reply(c, 200, "", "{\"guid\": \"%s\"}\n", lease_guid_new.c_str());
    }
    else
    {
        mg_http_reply(c, result.first, "", result.second.c_str());
    }
    
}

void PiTvServer::on_index_request(mg_connection* c, mg_http_message* hm)
{
    assert(hm);
    config.logger_ptr->info("{} request on /index.html URI!", hm->method.ptr);
    mg_http_reply(c, 200, "", "{\"result\": \"Hello World!\"}\n");
}

void PiTvServer::server_https_handler(mg_connection* c, int ev, void* ev_data, void* fn_data)
{
    PiTvServer* server = static_cast<PiTvServer*>(fn_data);

    if (ev == MG_EV_ACCEPT && fn_data != NULL)
    {
        struct mg_tls_opts opts = {
            .ca = mg_str(server->config.tls_ca_str.c_str()),
            .cert = mg_str(server->config.tls_pub_str.c_str()),
            .key = mg_str(server->config.tls_key_str.c_str()) };
        mg_tls_init(c, &opts);
    }
    else
    {
        server_http_handler(c, ev, ev_data, fn_data);
    }
}

std::string PiTvServer::addr_to_str(const mg_addr& addr)
{
    // TODO implement IPv6 correctly

    std::stringstream addr_builder;

    int lim = 4;
    if (addr.is_ip6)
    {
        lim = 16;
    }
    for (int i = 0; i < lim; i++)
    {
        addr_builder << (int)addr.ip[i];
        if (i < lim - 1)
        {
            addr_builder << ".";
        }
    }
    addr_builder << ":" << addr.port;
    return addr_builder.str();
}

PiTvServer::PiTvServer(const PiTvServerConfig& config, std::shared_ptr<Pipeline> pipeline)
{
    this->config = config;

    mg_log_set(MG_LL_DEBUG);
    mg_mgr_init(&mongoose_event_manager);

    mg_log_set_fn(&PiTvServer::mongoose_log_handler, this);

    log_level = config.logger_ptr->level();
    if (log_level == spdlog::level::off)
    {
        mg_log_set(0);
    }
    else if(log_level == spdlog::level::trace)
    {
        mg_log_set(4);
    }
    else if (log_level == spdlog::level::debug)
    {
        mg_log_set(3);
    }
    else if (log_level == spdlog::level::info || log_level == spdlog::level::warn)
    {
        mg_log_set(2);
    }
    else
    {
        mg_log_set(1);
    }

    // Since we cannot differentiate mg's messages, make them all at least INFO level
    log_level = config.logger_ptr->level();
    if (log_level < spdlog::level::level_enum::info)
    {
        log_level = spdlog::level::level_enum::info;
    }

    pipeline_main_ptr = pipeline;

    user_db = UserDb::userdb_factory(config.user_db, config.logger_ptr);
    if (!user_db)
    {
        config.logger_ptr->error("Failed to get user_db!");
    }

    system_stats_ok = system_stats_init();
    system_stats_cpu_temp_ok = system_stats_has_temp_cpu();
}

PiTvServer::~PiTvServer()
{
    mg_mgr_free(&mongoose_event_manager);
}

bool PiTvServer::start_server()
{
    if (is_server_running)
    {
        config.logger_ptr->warn("start_server() called for already running server!");
        return true;
    }

    is_server_running = true;

    for (auto http_addr : config.http_listeners)
    {
        mg_http_listen(&mongoose_event_manager, http_addr.c_str(), server_http_handler, this);
        config.logger_ptr->info("Listening on {}", http_addr);
    }

    for (auto https_addr : config.https_listeners)
    {
        mg_http_listen(&mongoose_event_manager, https_addr.c_str(), server_https_handler, this);
        config.logger_ptr->info("Listening on {}", https_addr);
    }

    config.logger_ptr->info("Adding time function");

    mg_timer_add(&mongoose_event_manager, 1000, MG_TIMER_REPEAT, timer_fn, this);

    return true;
}

bool PiTvServer::server_poll(int timeout_msec)
{
    mg_mgr_poll(&mongoose_event_manager, timeout_msec);
    return true;
}

std::string PiTvServer::gen_random_string(const int len)
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i)
    {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}

std::pair<int, std::string> PiTvServer::end_camera_lease(std::string username, std::string guid)
{
    if (username.empty())
    {
        config.logger_ptr->error("Lease end request failed: user not specified", guid);
        return { 401, "User not specified" };
    }

    if (user_map.count(username) == 0)
    {
        config.logger_ptr->error("Lease end request failed: user {} does not exist", username);
        return { 401, "User not found" };
    }

    PiTvUser& user = user_map[username];

    if (user.lease_map.count(guid) == 0)
    {
        config.logger_ptr->warn("Lease end request from {} tried to free non-existing lease {}", username, guid);
        return { 200, "No lease" };
    }

    LeaseEntry entry = user.lease_map[guid];
    user.lease_map.erase(guid);

    if (!pipeline_main_ptr->rtp_remove_endpoint(entry.udp_host, entry.udp_port))
    {
        config.logger_ptr->error("Lease end request from user {} failed: unable to detach RTP bin", username);
        return { 500, "Internal server error" };
    }

    return{ 200, "OK" };
}

std::pair<int, std::string> PiTvServer::lease_camera(std::string& guid, std::string username, std::string host, int port, uint64_t lease_time_msec)
{
    if (!pipeline_main_ptr)
    {
        config.logger_ptr->error("Lease request failed: pipeline_ptr is nullptr.", guid);
        return {500, "Internal server error" };
    }

    if (username.empty())
    {
        config.logger_ptr->error("Lease request failed: user not specified", guid);
        return { 401, "User not specified" };
    }

    if (user_map.count(username) == 0)
    {
        PiTvUser user_new;
        user_new.username = username;
        user_map[username] = user_new;
        config.logger_ptr->info("User {} was not in user map. Entry created.", username);
    }

    PiTvUser& user = user_map[username];

    uint64_t current_uptime = mg_millis();

    if (lease_time_msec > max_lease_time_msec)
    {
        config.logger_ptr->warn("Lease request has too big lease time {} msec!", lease_time_msec);
        lease_time_msec = max_lease_time_msec;
    }

    if (guid.empty())
    {
        if (user.lease_map.size() >= config.user_max_leases)
        {
            config.logger_ptr->error("Lease request failed: user {} reached maximum number of leases", username);
            return { 403, "Lease limit" };
        }

        std::string guid_new = gen_random_string(guid_length); 
        
        if (!pipeline_main_ptr->rtp_add_endpoint(host, port))
        {
            config.logger_ptr->error("Failed to create RTP bin!");
            return { 500, "Internal server error" };
        }

        LeaseEntry lease_entry;
        lease_entry.guid = guid_new;
        lease_entry.lease_end_time = current_uptime + lease_time_msec;
        lease_entry.udp_host = host;
        lease_entry.udp_port = port;
        lease_entry.user = username;
        user.lease_map[guid_new] = lease_entry;

        config.logger_ptr->info("Camera leased successfully to {}:{} with lease time {} msec, guid {} assigned!", host, port, lease_time_msec, guid_new);
        guid = guid_new;
    }
    else
    {
        if (user.lease_map.count(guid) == 0)
        {
            config.logger_ptr->error("Lease request failed: user {} requests non-existing GUID {}", username, guid);
            return { 400, "Non-existing GUID specified" };
        }

        LeaseEntry& lease_entry = user.lease_map[guid];
        lease_entry.lease_end_time = current_uptime + lease_time_msec;
        if (lease_entry.udp_host != host || lease_entry.udp_port != port)
        {
            config.logger_ptr->info("User {} requested endpoint change for lease {}", username, guid);
            if (!pipeline_main_ptr->rtp_change_endpoint(lease_entry.udp_host, lease_entry.udp_port, host, port))
            {
                config.logger_ptr->error("Endpoint change failed!");
                return { 500, "Internal server error" };
            }
            lease_entry.udp_host = host;
            lease_entry.udp_port = port;
        }
    }

    return { 200, "OK" };
}

PiTvServerStatus PiTvServer::get_server_status() const
{
    PiTvServerStatus status;
    if (system_stats_ok)
    {
        status.load_cpu_process_ok = true;
        status.load_cpu_total_ok = true;
        status.load_cpu_process = system_stats_get_cpu_process();
        status.load_cpu_total = system_stats_get_cpu_total();
    }

    if (system_stats_cpu_temp_ok)
    {
        status.temperature_cpu_ok = true;
        status.temperature_cpu = system_stats_get_temp_cpu();
    }

    return status;
}
