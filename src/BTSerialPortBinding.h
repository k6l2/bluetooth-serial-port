#pragma once

#include <string>
#include <memory>

struct bluetooth_data;

class BTSerialPortBinding
{
private:
	std::unique_ptr<bluetooth_data> data;
	std::string address;
	int channelID;
	BTSerialPortBinding(std::string address, int channelID);
	timeval* timeoutRead;
public:
	~BTSerialPortBinding();
	static BTSerialPortBinding *Create(std::string address, int channelID);
	void Connect();
	void Close();
	// returns # of bytes read, assuming no exceptions
	int Read(char *buffer, int length);
	// returns # of bytes sent, assuming no exceptions
	int Write(const char *buffer, int length);
	bool IsDataAvailable();
	// If seconds or microSeconds is < 0,
	//	the timeout object is set to nullptr
	void setTimoutRead(long seconds, long microSeconds = 0);
};
