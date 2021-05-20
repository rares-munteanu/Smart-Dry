#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;
using namespace Pistache;

class SmartDry
{
private:
    string status;

public:
    SmartDry()
    {
        status.assign("Off");
    };

    void statusRequest(const Rest::Request &, Http::ResponseWriter response)
    {
        json array_not_object = json({{"status", status}});
        auto mime = Http::Mime::MediaType::fromString("application/json");

        response.send(Http::Code::Ok, array_not_object.dump(), mime);
    };
    void initializeRequest(const Rest::Request &, Http::ResponseWriter response)
    {
        status.assign("Initialized");
        // More initialization stuff
        response.send(Http::Code::Ok);
    };
};

class SmartDryHandler
{
public:
    explicit SmartDryHandler(Address addr) : server(make_shared<Http::Endpoint>(addr))
    {
        smdr = new SmartDry();
        auto opts = Http::Endpoint::options().threads(1);
        server->init(opts);
        setRoutes();
        server->setHandler(router.handler());
    }

    void setRoutes()
    {
        using namespace Rest;
        Routes::Get(router, "/status", Routes::bind(&SmartDry::statusRequest, smdr));
        Routes::Get(router, "/initialize", Routes::bind(&SmartDry::initializeRequest, smdr));
    }

    void start()
    {
        server->serve();
    }

    void stop()
    {
        server->shutdown();
    }

private:
    SmartDry *smdr;
    shared_ptr<Http::Endpoint> server;
    Rest::Router router;
};

int main()
{
    Address addr(Ipv4::any(), Port(8080));
    SmartDryHandler sdHandler(addr);
    sdHandler.start();
}
