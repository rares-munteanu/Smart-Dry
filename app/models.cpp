#include <nlohmann/json.hpp>

using namespace std;

class Cloth {

private:
    string type;
    string color;

public:
    Cloth(const string &type, const string &color) : type(type), color(color) {}

    void setType(const string &type) {
        this->type = type;
    }

    void setColor(const string &color) {
        this->color = color;
    }

    [[nodiscard]] const string &getType() const {
        return type;
    }

    [[nodiscard]] const string &getColor() const {
        return color;
    }
};