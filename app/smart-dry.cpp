#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <nlohmann/json.hpp>

#include <pthread.h>
#include <chrono>
// #include <thread>

#include <vector>

#include "models.cpp"

using json = nlohmann::json;
using namespace std;
using namespace Pistache;

enum Status
{
    Stopped,
    Paused,
    Running
};

class Statistics
{
private:
    int rotations;
    int totalDuration;
    int clothesDryed;
    int totalTemp;
    int totalRPM;
    int numberOfDries;

public:
    Statistics()
    {
    }

    void addDry(int rpm, int temp, int duration, int numberOfClothes)
    {
        numberOfDries++;

        totalTemp += temp;
        totalRPM += rpm;
        totalDuration += duration;

        clothesDryed += numberOfClothes;

        rotations += duration * rpm;
    }

    json getStatistics()
    {
        if (numberOfDries == 0)
        {
            return json({{"Total rotations", rotations},
                         {"Total time drier has dried", totalDuration},
                         {"Average temperature", -1},
                         {"Average RPM", -1},
                         {"Average duration", -1},
                         {"Clothes dried", clothesDryed},
                         {"Number of dries", numberOfDries}});
        }

        return json({{"Total rotations", rotations},
                     {"Total time drier has dried", totalDuration},
                     {"Average temperature", (float)(totalTemp / numberOfDries)},
                     {"Average RPM", (float)(totalRPM / numberOfDries)},
                     {"Average duration", (float)(totalDuration / numberOfDries)},
                     {"Clothes dried", clothesDryed},
                     {"Number of dries", numberOfDries}});
    }
};

class SmartDry
{
private:
    // string status;
    Status status1;
    vector<Cloth> clothes;
    double perfume; // expressed as a percentage (x%)
    int maxWeight;
    int currentWeight;
    int durationLeft;

    Program currentProgram;
    Statistics stat;

    inline string getPerfumeFormatted()
    {
        return to_string(perfume) + "%";
    }

    inline json getClothes()
    {
        return Cloth::serializeVector(clothes);
    }

    Statistics &getStat()
    {
        return stat;
    }

    void reset()
    {
        perfume = 0;
        currentWeight = 0;
        durationLeft = 0;
        clothes.clear();
    }

    Program getProgram()
    {
        return currentProgram;
    }

public:
    static string statuses[3];
    string getStatusString(int enum_val)
    {
        string tmp(statuses[enum_val]);
        return tmp;
    }

    Status &getStatus()
    {
        return status1;
    }

    vector<Cloth> &getClothesVector()
    {
        return clothes;
    }

    int getPerfume()
    {
        return perfume;
    }

    int getDurationLeft()
    {
        return durationLeft;
    }

    void setDurationLeft(const int &_durationLeft)
    {
        durationLeft = _durationLeft;
    }

    void setPerfume(const double &_perfume)
    {
        perfume = _perfume;
    }

    int &getCurrentWeight()
    {
        return currentWeight;
    }
    void setCurrentWeight(const int &_currentWeight)
    {
        currentWeight = _currentWeight;
    }

    SmartDry()
    {
        // status.assign("off");
        status1 = Status::Stopped;
        perfume = 0;       // starts with no perfume
        maxWeight = 3000;  // 3 kg
        currentWeight = 0; // starts with 0

        currentProgram = Program(Loader::getInstance().getPrograms()[0]);
    };

    bool isRunning(Http::ResponseWriter *response)
    {
        if (status1 == Status::Running)
        {
            response->send(Http::Code::Bad_Request, "Machine is running. Pause/stop the program or wait until it is over.");
            return true;
        }
        return false;
    }

    bool isPaused(Http::ResponseWriter *response)
    {
        if (status1 == Status::Paused)
        {
            response->send(Http::Code::Bad_Request, "Machine is paused. Stop the program to adjust rpm/heat/duration/program.");
            return true;
        }
        return false;
    }

    static void *dryClothes(void *data)
    {
        SmartDry *smdr = (SmartDry *)data;

        for (;;)
        {
            if (smdr->getStatus() != Status::Running)
            {
                pthread_exit(NULL);
            }

            sleep(1);

            cout << "Drying clothes " << endl;

            smdr->setDurationLeft(smdr->getDurationLeft() - 1);

            smdr->setPerfume((smdr->getDurationLeft() * smdr->getPerfume()) / smdr->getProgram().getDuration());

            for (auto &cloth : smdr->getClothesVector())
            {
                cloth.setWet(double((smdr->getDurationLeft() * cloth.getWet()) / smdr->getProgram().getDuration()));
            }

            if (smdr->getDurationLeft() == 0)
            {
                smdr->status1 = Status::Stopped;
                smdr->currentWeight = 0;
                for (auto &cloth : smdr->getClothesVector())
                {
                    cloth.setWet(0);
                    smdr->currentWeight += cloth.getWeight();
                }

                // calculate statistics
                smdr->getStat().addDry(smdr->getProgram().getRPM(), smdr->getProgram().getHeat(), smdr->getProgram().getDuration(), smdr->clothes.size());

                pthread_exit(NULL);
            }
        }
    }

    void startSmartProgram(const Rest::Request &, Http::ResponseWriter response)
    {
        if (isRunning(&response))
            return;

        if (currentWeight == 0)
        {
            response.send(Http::Code::Bad_Request, "There are no clothes. Please load them before.");
            return;
        }
        Materials material = Loader::getInstance().getClothingMaterials();
        int heatToBeUsed = 0;
        for (auto &cloth : clothes)
        {
            int clothMaxHeat = material.at(cloth.getMaterial()).getMaxTemp();
            if (clothMaxHeat > heatToBeUsed)
            {
                heatToBeUsed = clothMaxHeat;
            }
        }
        this->currentProgram.setHeat(heatToBeUsed);
        this->currentProgram.setRPM(2000);

        double maxDuration = 0;
        for (auto &cloth : clothes)
        {
            double _currentClothDuration = cloth.getDryDuration(currentProgram.getHeat(), currentProgram.getRPM());
            // cout << "HEAT " << currentProgram.getHeat() << " RPM " << currentProgram.getRPM() << " maxDuration " << maxDuration << " _currentClothDuration " << _currentClothDuration << endl;
            maxDuration = max(maxDuration, _currentClothDuration);
        }
        this->currentProgram.setDuration(maxDuration);
        status1 = Status::Running;
        try
        {
            pthread_t thread;
            int rc = pthread_create(&thread, NULL, dryClothes, (void *)this);

            if (rc)
            {
                cout << "Error:unable to create thread," << rc << endl;
                status1 = Status::Stopped;
            }
        }
        catch (const std::exception &)
        {
            status1 = Status::Stopped;
            response.send(Http::Code::Bad_Request, string("Error:unable to create thread"));
            return;
        }

        response.send(Http::Code::Ok, "Program has started");
        durationLeft = currentProgram.getDuration();
    }

    void startProgram(const Rest::Request &, Http::ResponseWriter response)
    {

        if (isRunning(&response))
            return;

        if (currentWeight == 0)
        {
            response.send(Http::Code::Bad_Request, "There are no clothes. Please load them before.");
            return;
        }

        double maxDuration = 0;
        for (auto &cloth : clothes)
        {
            double _currentClothDuration = cloth.getDryDuration(currentProgram.getHeat(), currentProgram.getRPM());
            // cout << "HEAT " << currentProgram.getHeat() << " RPM " << currentProgram.getRPM() << " maxDuration " << maxDuration << " _currentClothDuration " << _currentClothDuration << endl;
            maxDuration = max(maxDuration, _currentClothDuration);
        }
        // cout << maxDuration << endl;

        if (maxDuration > currentProgram.getDuration())
        {
            ostringstream strs;
            strs << maxDuration;
            string str = strs.str();
            response.send(Http::Code::Bad_Request, string("Set duration is less than it should be. Recommended duration : bigger than ") + str);
            return;
        }

        // Creating new thread to handle drying process

        status1 = Status::Running;
        try
        {
            pthread_t thread;
            int rc = pthread_create(&thread, NULL, dryClothes, (void *)this);

            if (rc)
            {
                cout << "Error:unable to create thread," << rc << endl;
                status1 = Status::Stopped;
            }
        }
        catch (const std::exception &)
        {
            status1 = Status::Stopped;
            response.send(Http::Code::Bad_Request, string("Error:unable to create thread"));
            return;
        }

        response.send(Http::Code::Ok, "Program has started");
        durationLeft = currentProgram.getDuration();
        // status.assign("on");
    }

    void statusRequest(const Rest::Request &, Http::ResponseWriter response)
    {
        try
        {

            json _program = {
                {"heat",
                 currentProgram.getHeat()},
                {"rpm",
                 currentProgram.getRPM()},
                {"duration",
                 currentProgram.getDuration()},
            };

            json array_not_object = json({{"clothes", getClothes()},
                                          //   {"status", status},
                                          {"status", getStatusString(status1)},
                                          {"perfume", getPerfumeFormatted()},
                                          {"currentWeight", currentWeight},
                                          {"maxWeight", maxWeight},
                                          {"program", _program},
                                          {"durationLeft", durationLeft}});

            auto mime = Http::Mime::MediaType::fromString("application/json");
            response.send(Http::Code::Ok, array_not_object.dump(), mime);
        }
        catch (...)
        {
            response.send(Http::Code::Bad_Request, "An unexpected error occured");
        }
    }

    void initializeRequest(const Rest::Request &, Http::ResponseWriter response)
    {
        // status.assign("Initialized");
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
        if (isRunning(&response) || isPaused(&response))
            return;

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
                currentProgram = Program(it);

                response.send(Http::Code::Ok);
                return;
            }
        }

        response.send(Http::Code::Not_Found);
    }

    void addClothes(const Rest::Request &request, Http::ResponseWriter response)
    {
        if (isRunning(&response))
            return;
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
        Materials material = Loader::getInstance().getClothingMaterials();
        int clothesToAddWeight = this->currentWeight;
        for (Cloth &cloth : clothesToAdd)
        {
            if (material.at(cloth.getMaterial()).getMaxTemp() < this->currentProgram.getHeat())
            {
                response.send(Http::Code::Bad_Request, "You are trying to add clothes that doesn't allow this heat. Lower the heat.");
                return;
            }

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
        if (isRunning(&response))
            return;
        json requestBody = json::parse(request.body());

        for (auto clothId : requestBody)
        {
            if (clothId.is_number())
            {

                vector<Cloth>::iterator it;

                for (it = clothes.begin(); it != clothes.end(); ++it)
                {
                    if ((*it).getId() == clothId)
                    {
                        this->currentWeight -= (*it).getRealWeight();
                        clothes.erase(it);
                        break;
                    }
                }
            }
        }

        response.send(Http::Code::Ok);
    }

    void setHeat(const Rest::Request &request, Http::ResponseWriter response)
    {
        if (isRunning(&response) || isPaused(&response))
            return;

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
        currentProgram.setHeat(newHeat);
        response.send(Http::Code::Ok);
    }

    void setPerfumeR(const Rest::Request &request, Http::ResponseWriter response)
    {
        if (isRunning(&response) || isPaused(&response))
            return;
        json requestBody = json::parse(request.body());
        int newPerfume = 0;
        try
        {
            newPerfume = requestBody.at("perfume");
            if (newPerfume < 0 || newPerfume > 100)
            {
                throw runtime_error("Invalid value. It should be >= 0 and <=100.");
            }
        }
        catch (runtime_error e)
        {
            cout << "setPerfume - " << e.what() << endl;
            response.send(Http::Code::Bad_Request, e.what());
            return;
        }
        catch (...)
        {
            cout << "setPerfume - Invalid body format." << endl;
            response.send(Http::Code::Bad_Request, "Invalid body format.");
            return;
        }
        perfume = newPerfume;
        response.send(Http::Code::Ok);
    }

    void setRPM(const Rest::Request &request, Http::ResponseWriter response)
    {
        if (isRunning(&response) || isPaused(&response))
            return;

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
        currentProgram.setRPM(newRPM);
        response.send(Http::Code::Ok);
    }

    void setDuration(const Rest::Request &request, Http::ResponseWriter response)
    {
        if (isRunning(&response) || isPaused(&response))
            return;
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
        currentProgram.setDuration(newduration);
        response.send(Http::Code::Ok);
    }

    void changeStatus(const Rest::Request &request, Http::ResponseWriter response)
    {
        auto newStatus = request.param(":newStatus").as<string>();
        transform(newStatus.begin(), newStatus.end(), newStatus.begin(), ::tolower);
        string pause = "pause";
        string stop = "stop";
        if (newStatus.compare(pause) == 0)
        {
            if (status1 == Status::Running)
            {
                status1 = Status::Paused;
                response.send(Http::Code::Ok, "Machine is paused.");
                return;
            }
        }
        else if (newStatus.compare(stop) == 0)
        {
            status1 = Status::Stopped;
            response.send(Http::Code::Ok, "Machine is stopepd.");
            return;
        }
        else
        {
            response.send(Http::Code::Bad_Request, "Available query parameters are: stop and pause.");
            return;
        }

        response.send(Http::Code::Bad_Request, "Invalid operation.");
    }

    void
    getStatistics(const Rest::Request &request, Http::ResponseWriter response)
    {
        json resBody = stat.getStatistics();

        auto mime = Http::Mime::MediaType::fromString("application/json");
        response.send(Http::Code::Ok, resBody.dump(), mime);
    }
};

string SmartDry::statuses[3] = {"Off", "Pause", "On"};

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
        Routes::Get(router, "/initialize", Routes::bind(&SmartDry::initializeRequest, smdr));

        Routes::Get(router, "/start", Routes::bind(&SmartDry::startProgram, smdr));
        Routes::Get(router, "/start/smart", Routes::bind(&SmartDry::startSmartProgram, smdr));

        Routes::Get(router, "/status", Routes::bind(&SmartDry::statusRequest, smdr));

        Routes::Post(router, "/clothes", Routes::bind(&SmartDry::addClothes, smdr));
        Routes::Delete(router, "/clothes", Routes::bind(&SmartDry::removeClothes, smdr));

        Routes::Get(router, "/programs", Routes::bind(&SmartDry::getPrograms, smdr));
        Routes::Post(router, "/program", Routes::bind(&SmartDry::setProgram, smdr));
        Routes::Get(router, "/program/:newStatus", Routes::bind(&SmartDry::changeStatus, smdr));

        Routes::Post(router, "/heat", Routes::bind(&SmartDry::setHeat, smdr));
        Routes::Post(router, "/rpm", Routes::bind(&SmartDry::setRPM, smdr));
        Routes::Post(router, "/duration", Routes::bind(&SmartDry::setDuration, smdr));
        Routes::Post(router, "/perfume", Routes::bind(&SmartDry::setPerfumeR, smdr));

        Routes::Get(router, "/statistics", Routes::bind(&SmartDry::getStatistics, smdr));
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
