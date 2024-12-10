#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "SCanMessage.h"
#include "BusMaster.hpp"

bool ParseLine(std::string line, SCanMessage* message)
{
    //Example input:
    //10:52:48.266 CAN::Logger:10598,0,301,0,0,3,B5,0C,00

	//Only process lines that contain "CAN::Logger".
	if (line.find("CAN::Logger") == std::string::npos)
		return false;

    //Split the line by commas.
	size_t pos = 0;
	std::vector<std::string> tokens;
	while ((pos = line.find(',')) != std::string::npos)
	{
		tokens.push_back(line.substr(0, pos));
		line.erase(0, pos + 1);
	}
	tokens.push_back(line); //Add the remainder of the line to the tokens list.

	//A valid message should have at least 5 tokens.
	if (tokens.size() < 5)
		return false;

	message->bus = std::stoi(tokens[1]) == 0 ? false : true;
	message->id = std::stoi(tokens[2], nullptr, 16);
    message->isExtended = std::stoi(tokens[3]) == 0 ? false : true;
	message->isRemote = std::stoi(tokens[4]) == 0 ? false : true;
    message->length = std::stoi(tokens[5]);
	for (int i = 0; i < message->length; i++)
		message->data[i] = std::stoi(tokens[6 + i], nullptr, 16);

	return true;
}

int main()
{
    std::string filePath = "..\\valuable_recordings\\serial_20241205_110119.txt";
	//std::string filePath;

    //Prompt the user for the file path.
    if (filePath.empty())
    {
        std::cout << "Enter the path to the .txt file: ";
        std::getline(std::cin, filePath);
    }

    //I use the windows copy file path feature which wraps a file in quotation marks so remove these if they are present.
	if (filePath.front() == '\"' && filePath.back() == '\"')
		filePath = filePath.substr(1, filePath.size() - 2);

    //Check if the file exists.
    if (!std::filesystem::exists(filePath))
    {
        std::cerr << "File not found. Please check the path and try again." << std::endl;
        return 1;
    }

    //Open the file for reading.
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open the file!" << std::endl;
        return 2;
    }

    std::string line;
    while (std::getline(file, line))
    {
        SCanMessage message;
        if (!ParseLine(line, &message))
            continue;
        InterceptMessage(&message);
    }

    file.close();

    return 0;
}
