#include "json/json.hpp"
#include <iostream>
/**
 * \brief Parse a raw string into Value object using the CharReaderBuilder
 * class, or the legacy Reader class.
 * Example Usage:
 * $g++ readFromString.cpp -ljsoncpp -std=c++11 -o readFromString
 * $./readFromString
 * colin
 * 20
 */

#include <iostream>
#include <string>

int main()
{
	const std::string rawJson = R"({"Age": 20, "Name": "colin"})";
	const int rawJsonLength = static_cast<int>(rawJson.length());

 
//	std::string str1 = "0x9401a8c0";
	std::string str1 = "0x9401a8c0";
	std::size_t size;
	long long myint1 = std::stoll(str1, nullptr, 16);
	std::cout << "std::stoi(\"" << str1 << "\") is " << myint1 << '\n';

	
	JSONCPP_STRING err;
	Json::Value root;

	{
		Json::CharReaderBuilder builder;
		builder["collectComments"] = false;
		
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader->parse(rawJson.c_str(), rawJson.c_str() + rawJsonLength, &root, &err))
		{
			std::cout << "error" << std::endl;
			return EXIT_FAILURE;
		}
	}
	
	const std::string name = root["Name"].asString();
	const int age = root["Age"].asInt();

	std::cout << name << std::endl;
	std::cout << age << std::endl;
	return EXIT_SUCCESS;
}

