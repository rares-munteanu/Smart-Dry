#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <utility>
#include "storageModels.cpp"
#include <vector>

using namespace std;
using json = nlohmann::json;

using Materials = unordered_map<string, Material>;
using Colors = unordered_map<string, Color>;
using ClothTypes = unordered_map<string, ClothType>;

class Loader
{

private:
    Materials materialsMap;
    Colors colorsMap;
    ClothTypes clothesMap;
    vector<Program> programsVector;

    Loader()
    {
        cout << 1;
        json materials = getJsonFileContent("./storage/clothingMaterials.json");

        for (auto &it : materials)
        {
            materialsMap.insert(std::make_pair(it.at("name"), it));
        }

        cout << 2;
        json colors = getJsonFileContent("./storage/colorCombinations.json");

        for (auto &it : colors)
        {
            colorsMap.insert(std::make_pair(it.at("name"), it));
        }

        cout << 3;
        json clothes = getJsonFileContent("./storage/clothingWeights.json");

        for (auto &it : clothes)
        {
            string key = it.at("type").get<string>() + it.at("material").get<string>();
            transform(key.begin(), key.end(), key.begin(), ::tolower);
            clothesMap.insert(std::make_pair(key, it));
        }

        // cout << 4;
        // json programs = getJsonFileContent("./storage/progams.json");

        // for (auto &it : programs)
        // {
        //     programsVector.push_back(Program(it));
        // }
    }

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
        return materialsMap;
    }

    Colors getColors()
    {
        return colorsMap;
    }

    ClothTypes getClothTypes()
    {
        return clothesMap;
    }

    vector<Program> getPrograms()
    {
        return programsVector;
    }
};