#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <nlohmann/json.hpp>
#include <vector>

#include "models.cpp"

#include <vector>

using json = nlohmann::json;
using namespace std;
using namespace Pistache;

class SmartDry
{
private:
    string status;
    vector<Cloth> clothes;
    int perfume; // expressed as a percentage (x%)
    int maxWeight;
    int currentWeight;

    int rpm;
    int heat;
    int duration;
    int durationLeft;

    inline string getPerfume()
    {
        return to_string(perfume) + "%";
    }

    inline json getClothes()
    {
        return Cloth::serializeVector(clothes);
    }

public:
    SmartDry()
    {
        status.assign("off");
        perfume = 0;       // starts with no perfume
        maxWeight = 3000;  // 3 kg
        currentWeight = 0; // starts with 0

        Program defaultProgram = Loader::getInstance().getPrograms()[0];
        heat = defaultProgram.getHeat();
        duration = defaultProgram.getDuration();
        rpm = defaultProgram.getRPM();
    };

    void statusRequest(const Rest::Request &, Http::ResponseWriter response)
    {

        try
        {
            json array_not_object = json({{"status", status}, {"perfume", getPerfume()}, {"clothes", getClothes()}, {"currentWeight", currentWeight}, {"maxWeight", maxWeight}, {"heat", heat}, {"rpm", rpm}, {"duration", duration}, {"durationLeft", durationLeft}});
            auto mime = Http::Mime::MediaType::fromString("application/json");
            response.send(Http::Code::Ok, array_not_object.dump(), mime);
        }
        catch (...)
        {
            response.send(Http::Code::Bad_Request, "An unexpected error occured");
        }
    };

    void initializeRequest(const Rest::Request &, Http::ResponseWriter response)
    {
        status.assign("Initialized");
        // More initialization stuff
        response.send(Http::Code::Ok);
    };

    void getPrograms(const Rest::Request &request, Http::ResponseWriter response)
    {
        vector<Program> programs = Loader::getInstance().getPrograms();
        auto mime = Http::Mime::MediaType::fromString("application/json");

        json programsJson = json();
        for (auto &it : programs)
        {
            cout << it.getName() << endl;
            programsJson.push_back(it.serialize());
        }

        response.send(Http::Code::Ok, programsJson.dump(), mime);
    }

    void setProgram(const Rest::Request &request, Http::ResponseWriter response)
    {
        vector<Program> programs = Loader::getInstance().getPrograms();

        json requestBody = json::parse(request.body());
        string programToSelect;
        try
        {
            programToSelect = requestBody.at("program");
        }
        catch (...)
        {
            cout << "setProgram - Invalid body format." << endl;
            response.send(Http::Code::Bad_Request, "Invalid body format.");
            return;
        }

        for (auto &it : programs)
        {
            if (it.getName() == programToSelect)
            {
                heat = it.getHeat();
                rpm = it.getRPM();
                duration = it.getDuration();

                response.send(Http::Code::Ok);
                return;
            }
        }

        response.send(Http::Code::Not_Found);
    }

    void addClothes(const Rest::Request &request, Http::ResponseWriter response)
    {
        string requestBody = request.body();
        vector<Cloth> clothesToAdd;
        try
        {
            clothesToAdd = Cloth::deserializeVector(requestBody);
        }
        catch (out_of_range e)
        {
            cout << "addClothes - Invalid combination of type and material" << endl;
            response.send(Http::Code::Bad_Request, "Invalid combination of type and material");
            return;
        }
        catch (runtime_error e)
        {
            cout << "addClothes - " << e.what() << endl;
            response.send(Http::Code::Bad_Request, e.what());
            return;
        }
        catch (...)
        {
            cout << "addClothes - Unexpected error in addClothes." << endl;
            response.send(Http::Code::Bad_Request, "Unexpected error in addClothes.");
            return;
        }
        int clothesToAddWeight = this->currentWeight;
        for (Cloth &cloth : clothesToAdd)
        {
            clothesToAddWeight += cloth.getRealWeight();
            if (clothesToAddWeight > this->maxWeight)
            {
                response.send(Http::Code::Bad_Request, "Overweight! Please try again with fewer clothes.");
                return;
            }
        }
        this->currentWeight = clothesToAddWeight;
        this->clothes.insert(clothes.end(), clothesToAdd.begin(), clothesToAdd.end());
        auto mime = Http::Mime::MediaType::fromString("application/json");

        response.send(Http::Code::Ok, this->getClothes().dump(), mime);
    }

    void removeClothes(const Rest::Request &request, Http::ResponseWriter response)
    {
        json requestBody = json::parse(request.body());

        for (auto clothId : requestBody)
        {
            if (clothId.is_number())
            {
                auto new_end = remove_if(clothes.begin(), clothes.begin(), [&](Cloth &cloth) -> bool {
                    return clothId == cloth.getId();
                });
                if (new_end != clothes.end())
                {
                    this->currentWeight -= new_end->getRealWeight();
                }
                clothes.erase(new_end, clothes.end());
            }
        }

        response.send(Http::Code::Ok);
    }

    void setHeat(const Rest::Request &request, Http::ResponseWriter response)
    {
        json requestBody = json::parse(request.body());
        int newHeat = 0;
        try
        {
            newHeat = requestBody.at("heat");
            if (newHeat < 30 || newHeat > 90)
            {
                throw runtime_error("Invalid value. It should be >= 30 and <=90.");
            }
            Materials material = Loader::getInstance().getClothingMaterials();
            for (auto cloth : this->clothes)
            {
                if (material.at(cloth.getMaterial()).getMaxTemp() < newHeat)
                {
                    throw runtime_error("Invalid value. There are clothes that doesn't allow this heat.");
                }
            }
        }
        catch (runtime_error e)
        {
            cout << "setHeat - " << e.what() << endl;
            response.send(Http::Code::Bad_Request, e.what());
            return;
        }
        catch (...)
        {
            cout << "setHeat - Invalid body format." << endl;
            response.send(Http::Code::Bad_Request, "Invalid body format.");
            return;
        }
        this->heat = newHeat;
        response.send(Http::Code::Ok);
    }

    void setRPM(const Rest::Request &request, Http::ResponseWriter response)
    {
        json requestBody = json::parse(request.body());
        int newRPM = 0;
        try
        {
            newRPM = requestBody.at("rpm");
            if (newRPM < 400 || newRPM > 2000)
            {
                throw runtime_error("Invalid value. It should be >= 400 and <=2000.");
            }
        }
        catch (runtime_error e)
        {
            cout << "setRPM - " << e.what() << endl;
            response.send(Http::Code::Bad_Request, e.what());
            return;
        }
        catch (...)
        {
            cout << "setRPM - Invalid body format." << endl;
            response.send(Http::Code::Bad_Request, "Invalid body format.");
            return;
        }
        this->rpm = newRPM;
        response.send(Http::Code::Ok);
    }

    void setDuration(const Rest::Request &request, Http::ResponseWriter response)
    {
        json requestBody = json::parse(request.body());
        int newduration = 0;
        try
        {
            newduration = requestBody.at("duration");
            if (newduration < 30 || newduration > 240)
            {
                throw runtime_error("Invalid value. It should be >= 30 minutes and <=240 minutes.");
            }
        }
        catch (runtime_error e)
        {
            cout << "setDuration - " << e.what() << endl;
            response.send(Http::Code::Bad_Request, e.what());
            return;
        }
        catch (...)
        {
            cout << "setDuration - Invalid body format." << endl;
            response.send(Http::Code::Bad_Request, "Invalid body format.");
            return;
        }
        this->duration = newduration;
        response.send(Http::Code::Ok);
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
        Routes::Post(router, "/clothes", Routes::bind(&SmartDry::addClothes, smdr));
        Routes::Delete(router, "/clothes", Routes::bind(&SmartDry::removeClothes, smdr));
        Routes::Get(router, "/programs", Routes::bind(&SmartDry::getPrograms, smdr));
        Routes::Post(router, "/program", Routes::bind(&SmartDry::setProgram, smdr));
        Routes::Post(router, "/heat", Routes::bind(&SmartDry::setHeat, smdr));
        Routes::Post(router, "/rpm", Routes::bind(&SmartDry::setRPM, smdr));
        Routes::Post(router, "/duration", Routes::bind(&SmartDry::setDuration, smdr));
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
