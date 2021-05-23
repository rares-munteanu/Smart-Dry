#include <nlohmann/json.hpp>
#include <unordered_map>
#include <utility>

using namespace std;
using json = nlohmann::json;

class Material
{
private:
    string name;
    int dryFactor;
    int maxTemp;

public:
    Material(json material)
    {
        name = material.at("name");
        dryFactor = material.at("dryFactor");
        maxTemp = material.at("maxTemp");
    }

    const string &getName() const
    {
        return name;
    }

    const int &getDryFactor() const
    {
        return dryFactor;
    }

    const int &getMaxTemp() const
    {
        return maxTemp;
    }
};

struct ColorInteraction
{
    string name;
    string output;
};

class Color
{
private:
    string name;
    unordered_map<string, ColorInteraction> interactions;

public:
    Color(json color)
    {
        name = color.at("name");

        for (auto &it : color.at("interactions"))
        {
            interactions.insert(std::make_pair(it.at("name"), (ColorInteraction){it.at("name"), it.at("output")}));
        }
    }

    const string &getName() const
    {
        return name;
    }

    const unordered_map<string, ColorInteraction> &getInteractions() const
    {
        return interactions;
    }
};

class ClothType
{
private:
    string type;
    string material;
    int weight;

public:
    ClothType(json cloth)
    {
        type = cloth.at("type");
        material = cloth.at("material");
        weight = cloth.at("weight");
    }

    const string &getType() const
    {
        return type;
    }

    const string &getMaterial() const
    {
        return material;
    }

    const int &getWeight() const
    {
        return weight;
    }
};