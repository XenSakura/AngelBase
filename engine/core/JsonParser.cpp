module;
#include <simdjson.h>
#include <simdjson/ondemand.h>
#include <simdjson/padded_string.h>
export module JsonParser;

//got too annoying to write simdjson each time, but many of the types here are from this namespace
using namespace simdjson;

export using JsonObject = ondemand::object;
export class JsonParser
{
private:
    ondemand::parser parser;
public:
    JsonParser()
    {
        
    }

    ~JsonParser()
    {
        
    }

    

    void LoadJsonFile(const std::string& file)
    {
        auto json = padded_string::load(file.c_str());
        ondemand::document doc = parser.iterate(json);
        JsonObject root = doc.get_object();
        uint32_t total_params = root.count_fields();
        
    }
};

