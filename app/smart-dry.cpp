#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <nlohmann/json.hpp>

#include "models.cpp"

#include <vector>

using json = nlohmann::json;
using namespace std;
using namespace Pistache;
class Program
{
private:
    string name;
    int duration; // expresed in minutes
    int heat;     // expressed in celsius
    int rpm;      // expresses in rotations per minutes

public:
    Program(const string &name) : name(name) {}
};

class SmartDry
{
private:
    string status;
    int power; // expresed as a percentage (x%)
    vector<Cloth> clothes;
    int detergent; // expressed as a percentage (x%)
    int maxWeight;
    int currentWeight;

    inline string getPower()
    {
        return to_string(power) + "%";
    }

    inline string getDetergent()
    {
        return to_string(detergent) + "%";
    }

    inline vector<json> getClothes()
    {
        return Cloth::serializeVector(clothes);
    }

public:
    SmartDry()
    {
        status.assign("Off");
        power = 100;       // starts as fully charged
        detergent = 0;     // starts with no detergent
        maxWeight = 3000;  // 3 kg
        currentWeight = 0; // starts with 0
    };

    void statusRequest(const Rest::Request &, Http::ResponseWriter response)
    {
        json array_not_object = json({{"status", status}, {"power", getPower()}, {"detergent", getDetergent()}, {"clothes", getClothes()}, {"currentWeight", currentWeight}, {"maxWeight", maxWeight}});
        auto mime = Http::Mime::MediaType::fromString("application/json");

        response.send(Http::Code::Ok, array_not_object.dump(), mime);
    };
    void initializeRequest(const Rest::Request &, Http::ResponseWriter response)
    {
        status.assign("Initialized");
        // More initialization stuff
        response.send(Http::Code::Ok);
    };

    void addClothes(const Rest::Request &request, Http::ResponseWriter response)
    {
        string requestBody = request.body();
        vector<Cloth> clothesToAdd;
        try
        {
            clothesToAdd = Cloth::deserializeVector(requestBody);
        }
        catch (runtime_error e)
        {
            cout << e.what() << endl;
            response.send(Http::Code::Bad_Request, e.what());
            return;
        }
        catch (...)
        {
            cout << "Unexpected error in addClothes.";
            response.send(Http::Code::Bad_Request, "Unexpected error in addClothes.");
            return;
        }
        int clothesToAddWeight = this->currentWeight;
        for (const auto &cloth : clothesToAdd)
        {
            clothesToAddWeight += cloth.getWeight();
            if (clothesToAddWeight > this->maxWeight)
            {
                response.send(Http::Code::Bad_Request, "Overweight! Please try again with fewer clothes.");
                return;
            }
        }
        this->currentWeight = clothesToAddWeight;
        this->clothes.insert(clothes.end(), clothesToAdd.begin(), clothesToAdd.end());

        statusRequest(request, response);
    }
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
        Routes::Post(router, "/addClothes", Routes::bind(&SmartDry::addClothes, smdr));
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
