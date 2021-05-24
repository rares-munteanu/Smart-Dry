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
        json materials = getJsonFileContent("./storage/clothingMaterials.json");

        for (auto &it : materials)
        {

            string materialName = it.at("name");
            transform(materialName.begin(), materialName.end(), materialName.begin(), ::tolower);

            materialsMap.insert(std::make_pair(materialName, Material(it)));
        }

        json colors = getJsonFileContent("./storage/colorCombinations.json");

        for (auto &it : colors)
        {
            colorsMap.insert(std::make_pair(it.at("name"), Color(it)));
        }

        json clothes = getJsonFileContent("./storage/clothingWeights.json");

        for (auto &it : clothes)
        {
            string key = it.at("type").get<string>() + it.at("material").get<string>();
            transform(key.begin(), key.end(), key.begin(), ::tolower);
            clothesMap.insert(std::make_pair(key, ClothType(it)));
        }

        json programs = getJsonFileContent("./storage/programs.json");

        for (auto &it : programs)
        {
            programsVector.push_back(Program(it));
        }
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