#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <utility>
#include "storageModels.cpp"

using namespace std;
using json = nlohmann::json;

using Materials = unordered_map<string, Material>;
using Colors = unordered_map<string, Color>;
using ClothTypes = unordered_map<string, ClothType>;

class Loader
{

private:
    Loader() {}

    json getJsonFileContent(string fileName)
    {
        std::ifstream file(fileName);
        json j;

        file >> j;

        return j;
    }

public:
    Loader(Loader const &) = delete;
    void operator=(Loader const &) = delete;

    static Loader &getInstance()
    {
        static Loader instance;
        return instance;
    }

    Materials getClothingMaterials()
    {
        json materials = getJsonFileContent("./clothingMaterials.json");
        Materials materialsMap;

        for (auto &it : materials)
        {
            materialsMap.insert(std::make_pair(it.at("name"), it));
        }

        return materialsMap;
    }

    Colors getColors()
    {
        json colors = getJsonFileContent("./colorCombinations.json");
        Colors colorsMap;

        for (auto &it : colors)
        {
            colorsMap.insert(std::make_pair(it.at("name"), it));
        }

        return colorsMap;
    }

    ClothTypes getClothTypes()
    {
        json clothes = getJsonFileContent("./clothingWeights.json");
        ClothTypes clothesMap;

        for (auto &it : clothes)
        {
            clothesMap.insert(std::make_pair(it.at("type"), it));
        }

        return clothesMap;
    }
};