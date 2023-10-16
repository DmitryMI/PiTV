#include "PiTvServer.h"


void PiTvServer::server_http_handler(mg_connection* c, int ev, void* ev_data, void* fn_data)
{
    PiTvServer* server = static_cast<PiTvServer*>(fn_data);

    if (ev == MG_EV_HTTP_MSG)
    {
        mg_http_message* hm = (struct mg_http_message*)ev_data;
        if (mg_http_match_uri(hm, server->config.pitv_mount_point.c_str()))
        {
            server->on_pitv_get(c, hm);
        }
        else if (mg_http_match_uri(hm, "/index.html"))
        {
            server->on_index_get(c, hm);
        }
        else
        {
            struct mg_http_serve_opts opts = { .root_dir = server->config.recording_path.c_str()};
            mg_http_serve_dir(c, (mg_http_message*)ev_data, &opts);
        }
    }
}

void PiTvServer::on_pitv_get(mg_connection* c, mg_http_message* hm)
{
    config.logger_ptr->info("GET request on /camera URI!");
    mg_http_reply(c, 200, "", "{\"result\": \"%.*s\"}\n", (int)hm->uri.len, hm->uri.ptr);
}

void PiTvServer::on_index_get(mg_connection* c, mg_http_message* hm)
{
    config.logger_ptr->info("GET request on /index.html URI!");
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
            .key = mg_str(server->config.tls_priv_str.c_str()) };
        mg_tls_init(c, &opts);
    }
    else
    {
        server_http_handler(c, ev, ev_data, fn_data);
    }
}

PiTvServer::PiTvServer(const PiTvServerConfig& config)
{
    this->config = config;

    mg_log_set(MG_LL_DEBUG);
    mg_mgr_init(&mongoose_event_manager);
}

PiTvServer::~PiTvServer()
{
    mg_mgr_free(&mongoose_event_manager);
}

bool PiTvServer::start_server()
{
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

    return true;
}

bool PiTvServer::server_poll(int timeout_msec)
{
    mg_mgr_poll(&mongoose_event_manager, timeout_msec);
    return true;
}