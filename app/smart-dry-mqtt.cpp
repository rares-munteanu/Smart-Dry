#include <nlohmann/json.hpp>
#include <pthread.h>
#include <chrono>
// #include <thread>

#include <vector>
#include <functional>
#include "mqtt/async_client.h"

#include "models.cpp"
using json = nlohmann::json;
using namespace std;

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

    std::string initializeRequest(const std::string &request)
    {
        std::cout << "in init req: " << request << std::endl;
        // status.assign("Initialized");
        // More initialization stuff
        const std::string response = "ok";
        return response;
    };

    bool isRunning()
    {
        if (status1 == Status::Running)
        {
            return true;
        }
        return false;
    }

    bool isPaused()
    {
        if (status1 == Status::Paused)
        {
            return true;
        }
        return false;
    }

    std::string startSmartProgram(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";

        if (currentWeight == 0)
        {
            return "There are no clothes. Please load them before.";
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
            return "Error:unable to create thread";
        }

        durationLeft = currentProgram.getDuration();
        return "Program has started";
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

            // sleep(1);

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

    std::string statusRequest(const std::string &request)
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

            return array_not_object.dump();
        }
        catch (...)
        {
            return "An unexpected error occured";
        }
    }

    std::string addClothes(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";
        
        string requestBody = request;
        vector<Cloth> clothesToAdd;
        try
        {
            clothesToAdd = Cloth::deserializeVector(requestBody);
        }
        catch (out_of_range e)
        {
            cout << "addClothes - Invalid combination of type and material" << endl;
            return "Invalid combination of type and material";
        }
        catch (runtime_error e)
        {
            cout << "addClothes - " << e.what() << endl;
            return e.what();
        }
        catch (...)
        {
            cout << "addClothes - Unexpected error in addClothes." << endl;
            return "Unexpected error in addClothes.";
        }
        Materials material = Loader::getInstance().getClothingMaterials();
        int clothesToAddWeight = this->currentWeight;
        for (Cloth &cloth : clothesToAdd)
        {
            if (material.at(cloth.getMaterial()).getMaxTemp() < this->currentProgram.getHeat())
            {
                return "You are trying to add clothes that doesn't allow this heat. Lower the heat.";
            }

            clothesToAddWeight += cloth.getRealWeight();
            if (clothesToAddWeight > this->maxWeight)
            {
                return "Overweight! Please try again with fewer clothes.";
            }
        }
        this->currentWeight = clothesToAddWeight;
        this->clothes.insert(clothes.end(), clothesToAdd.begin(), clothesToAdd.end());

        return this->getClothes().dump();
    }

    std::string removeClothes(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";

        json requestBody = json::parse(request);

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

        return "ok";
    }

    std::string setHeat(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";
        if (isPaused())
            return "Machine is paused. Stop the program to adjust rpm/heat/duration/program.";

        json requestBody = json::parse(request);
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
            return e.what();
        }
        catch (...)
        {
            cout << "setHeat - Invalid body format." << endl;
            return "Invalid body format.";
        }
        currentProgram.setHeat(newHeat);
        return "ok";
    }

    std::string startProgram(const std::string &request)
    {

        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";

        if (currentWeight == 0)
        {
            return "There are no clothes. Please load them before.";
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
            return std::string("Set duration is less than it should be. Recommended duration : bigger than ") + std::to_string(maxDuration);
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
            return "Error:unable to create thread";
        }

        durationLeft = currentProgram.getDuration();
        // status.assign("on");
        return "Program has started";
    }

    std::string getPrograms(const std::string &request)
    {

        vector<Program> programs = Loader::getInstance().getPrograms();

        json programsJson = json();
        for (auto &it : programs)
        {
            cout << it.getName() << endl;
            programsJson.push_back(it.serialize());
        }

        return programsJson.dump();
    }

    std::string setProgram(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";
        if (isPaused())
            return "Machine is paused. Stop the program to adjust rpm/heat/duration/program.";

        vector<Program> programs = Loader::getInstance().getPrograms();

        json requestBody = json::parse(request);
        string programToSelect;
        try
        {
            programToSelect = requestBody.at("program");
        }
        catch (...)
        {
            cout << "setProgram - Invalid body format." << endl;
            return "Invalid body format";
        }

        for (auto &it : programs)
        {
            if (it.getName() == programToSelect)
            {
                currentProgram = Program(it);

                return "ok";
            }
        }

        return "not found";
    }

    std::string setRPM(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";
        if (isPaused())
            return "Machine is paused. Stop the program to adjust rpm/heat/duration/program.";

        json requestBody = json::parse(request);
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
            return e.what();
        }
        catch (...)
        {
            cout << "setRPM - Invalid body format." << endl;
            return "Invalid body format.";
        }
        currentProgram.setRPM(newRPM);
        return "ok";
    }

    std::string setDuration(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";
        if (isPaused())
            return "Machine is paused. Stop the program to adjust rpm/heat/duration/program.";

        json requestBody = json::parse(request);
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
            return e.what();
        }
        catch (...)
        {
            cout << "setDuration - Invalid body format." << endl;
            return "Invalid body format";
        }
        currentProgram.setDuration(newduration);
        return "ok";
    }

    std::string setPerfumeR(const std::string &request)
    {
        if (isRunning())
            return "Machine is running. Pause/stop the program or wait until it is over.";
        if (isPaused())
            return "Machine is paused. Stop the program to adjust rpm/heat/duration/program.";

        json requestBody = json::parse(request);
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
            return e.what();
        }
        catch (...)
        {
            cout << "setPerfume - Invalid body format." << endl;
            return "Invalid body format";
        }
        perfume = newPerfume;
        return "ok";
    }

    std::string getStatistics(const std::string &request)
    {
        json resBody = stat.getStatistics();
        return resBody.dump();
    }

    std::string changeStatus(const std::string &request)
    {
        auto newStatus = request;
        transform(newStatus.begin(), newStatus.end(), newStatus.begin(), ::tolower);
        string pause = "pause";
        string stop = "stop";
        if (newStatus.compare(pause) == 0)
        {
            if (status1 == Status::Running)
            {
                status1 = Status::Paused;
                return "Machine is paused";
            }
        }
        else if (newStatus.compare(stop) == 0)
        {
            status1 = Status::Stopped;
            return "Machine is stopepd";
        }
        else
        {
            return "Available query parameters are: stop and pause";
        }

        return "Invalid operation";
    }

};



string SmartDry::statuses[3] = {"Off", "Pause", "On"};


class ActionListener : public virtual mqtt::iaction_listener
{
	std::string name_;

	void on_failure(const mqtt::token& tok) override {
		std::cout << name_ << " failure";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		std::cout << std::endl;
	}

	void on_success(const mqtt::token& tok) override {
		std::cout << name_ << " success";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		auto top = tok.get_topics();
		if (top && !top->empty())
			std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
		std::cout << std::endl;
	}

public:
	ActionListener(const std::string& name) : name_(name) {}
};

class SmartDryHandler : public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
    const int QOS = 1;
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    ActionListener subListener_;

    SmartDry *smdr;
    std::vector <std::pair <std::string, std::function <std::string(std::string)>>> routes_;


    void reconnect() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            exit(1);
        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override {
        std::cout << "Connection attempt failed" << std::endl;
        if (++nretry_ > 20)
            exit(1);
        reconnect();
    }

    void on_success(const mqtt::token& tok) override {}
    
    void connection_lost(const std::string& cause) override {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}


    void connected(const std::string& cause) override {
        std::cout << "\nConnection success" << std::endl;
        std::cout << "\nSubscribing to topic '" << std::endl;

        for (auto &route : routes_) {
            cli_.subscribe(route.first, QOS, nullptr, subListener_);
        }
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::cout << "Message arrived" << std::endl;
        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;

        auto handler = std::find_if(routes_.begin(), routes_.end(), [&] (decltype(routes_)::value_type &route) {return route.first == msg->get_topic();});
        if (handler != routes_.end()) {
            auto response = handler->second(msg->to_string());
            publish("res/" + msg->get_topic(), response);
        }
    }


public:

    explicit SmartDryHandler(mqtt::async_client& cli, mqtt::connect_options& connOpts)
                : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("subscribe")
    {
        smdr = new SmartDry();
        routes_.push_back({"smart_dry/initialize", std::bind(&SmartDry::initializeRequest, smdr, std::placeholders::_1)});

        routes_.push_back({"smart_dry/start", std::bind(&SmartDry::startProgram, smdr, std::placeholders::_1)});
        routes_.push_back({"smart_dry/start_smart", std::bind(&SmartDry::startSmartProgram, smdr, std::placeholders::_1)});

        routes_.push_back({"smart_dry/status", std::bind(&SmartDry::statusRequest, smdr, std::placeholders::_1)});

        routes_.push_back({"smart_dry/set_clothes", std::bind(&SmartDry::addClothes, smdr, std::placeholders::_1)});
        routes_.push_back({"smart_dry/delete_clothes", std::bind(&SmartDry::removeClothes, smdr, std::placeholders::_1)});

        routes_.push_back({"smart_dry/get_programs", std::bind(&SmartDry::getPrograms, smdr, std::placeholders::_1)});
        routes_.push_back({"smart_dry/set_program", std::bind(&SmartDry::setProgram, smdr, std::placeholders::_1)});
        routes_.push_back({"smart_dry/set_program_status", std::bind(&SmartDry::changeStatus, smdr, std::placeholders::_1)});
        
        routes_.push_back({"smart_dry/set_heat", std::bind(&SmartDry::setHeat, smdr, std::placeholders::_1)});
        routes_.push_back({"smart_dry/set_rpm", std::bind(&SmartDry::setRPM, smdr, std::placeholders::_1)});
        routes_.push_back({"smart_dry/set_duration", std::bind(&SmartDry::setDuration, smdr, std::placeholders::_1)});
        routes_.push_back({"smart_dry/perfume", std::bind(&SmartDry::setPerfumeR, smdr, std::placeholders::_1)});

        routes_.push_back({"smart_dry/stats", std::bind(&SmartDry::getStatistics, smdr, std::placeholders::_1)});
    }

    void publish(const std::string &topic, const std::string& payload)
    {
        const int QOS = 1;
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(QOS);
        cli_.publish(msg);
    }
};



int main()
{
    const std::string addr {"tcp://localhost:1883"};
    const std::string clientId {"smart-dry-X033jh-2"};

    mqtt::async_client cli(addr, clientId);

    mqtt::connect_options connOpts;
    connOpts.set_clean_session(false);

    // Install the callback(s) before connecting.
    SmartDryHandler cb(cli, connOpts);
    cli.set_callback(cb);

    try {
        std::cout << "Connecting to the MQTT server..." << std::flush;
        cli.connect(connOpts, nullptr, cb);
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "\nERROR: Unable to connect to MQTT server: '"
            << addr << "'" << exc << std::endl;
        return 1;
    }

    // Just block till user tells us to quit.

    while (std::tolower(std::cin.get()) != 'q')
        ;

    // Disconnect

    try {
        std::cout << "\nDisconnecting from the MQTT server..." << std::flush;
        cli.disconnect()->wait();
        std::cout << "OK" << std::endl;
    }
    catch (const mqtt::exception& exc) {
        std::cerr << exc << std::endl;
        return 1;
    }

    // SmartDryHandler sdHandler(addr);
    // sdHandler.start();
}