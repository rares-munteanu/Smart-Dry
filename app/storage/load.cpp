#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

class Loader
{

protected:
    Loader(const std::string value) : value_(value) {}

    static Loader *loader_;

public:
    Loader(Loader &other) = delete;

    void operator=(const Loader &) = delete;

    static Loader *GetInstance(const std::string &value);

    /**
     * Finally, any singleton should define some business logic, which can be
     * executed on its instance.
     */

    void getJsonFileContent(string fileName)
    {
        std::ifstream file(fileName);
        json j;

        file >> j;

        return j;
    }
};