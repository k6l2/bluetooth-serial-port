#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <string>
#include <sstream>
#include <stdlib.h>
#include "../BluetoothException.h"
#include "../BTSerialPortBinding.h"
#include "BluetoothHelpers.h"

struct bluetooth_data
{
	SOCKET s;
	bool initialized;
};

using namespace std;

BTSerialPortBinding *BTSerialPortBinding::Create(string address, int channelID)
{
	if (channelID <= 0)
		throw BluetoothException("ChannelID should be a positive int value");

	auto obj = new BTSerialPortBinding(address, channelID);

	if (!obj->data->initialized)
	{
		delete obj;
		throw BluetoothException("Unable to initialize socket library");
	}

	return obj;
}

BTSerialPortBinding::BTSerialPortBinding(string address, int channelID)
	: address(address), channelID(channelID), data(new bluetooth_data())
	, timeoutRead(nullptr)
{
	data->s = INVALID_SOCKET;
	data->initialized = BluetoothHelpers::Initialize();
}

BTSerialPortBinding::~BTSerialPortBinding()
{
	if (data->initialized)
		BluetoothHelpers::Finalize();
}

void BTSerialPortBinding::Connect()
{
	Close();

	int status = SOCKET_ERROR;

	data->s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);

	if (data->s != SOCKET_ERROR)
	{
		SOCKADDR_BTH addr = { 0 };
		int addrSize = sizeof(SOCKADDR_BTH);
		TCHAR addressBuffer[40];

		if (address.length() >= 40)
			throw BluetoothException("Address length is invalid");

		for (size_t i = 0; i < address.length(); i++)
			addressBuffer[i] = (TCHAR)address[i];

		addressBuffer[address.length()] = 0;

		status = WSAStringToAddress(addressBuffer, AF_BTH, nullptr, (LPSOCKADDR)&addr, &addrSize);

		if (status != SOCKET_ERROR)
		{
			addr.port = channelID;
			status = connect(data->s, (LPSOCKADDR)&addr, addrSize);

			if (status != SOCKET_ERROR)
			{
				unsigned long enableNonBlocking = 1;
				ioctlsocket(data->s, FIONBIO, &enableNonBlocking);
			}
		}
	}

	if (status != 0)
	{
		string message = BluetoothHelpers::GetWSAErrorMessage(WSAGetLastError());

		if (data->s != INVALID_SOCKET)
			closesocket(data->s);

		throw BluetoothException("Cannot connect: " + message);
	}
}

void BTSerialPortBinding::Close()
{
	if (data->s != INVALID_SOCKET)
	{
		closesocket(data->s);
		data->s = INVALID_SOCKET;
	}
}

int BTSerialPortBinding::Read(char *buffer, int length)
{
	if (data->s == INVALID_SOCKET)
		throw BluetoothException("connection has been closed");

	if (buffer == nullptr)
		throw BluetoothException("buffer cannot be null");

	if (length == 0)
		return 0;

	fd_set set;
	FD_ZERO(&set);
	FD_SET(data->s, &set);

	int size = -1;

	const int iResult = select(
		static_cast<int>(data->s) + 1, &set, nullptr, nullptr, timeoutRead);
	if (iResult == SOCKET_ERROR)
	{
		const int socketError = WSAGetLastError();
		std::stringstream ss;
		ss << "Socket error =" << socketError;
		Close();
		throw BluetoothException(ss.str());
	}
	if (iResult == 0)
	{
		// if a timeout occurs, we consider the connection to be closed //
		Close();
		throw BluetoothException("time limit expired!");
	}
	else
	{
		if (FD_ISSET(data->s, &set))
		{
			size = recv(data->s, buffer, length, 0);
		}
		else 
		{
			// when no data is read from rfcomm the connection has been closed.
			size = 0;
			Close();
		}
	}

	if (size < 0)
		throw BluetoothException("Error reading from connection");

	return size;
}

void BTSerialPortBinding::Write(const char *buffer, int length)
{
	if (buffer == nullptr)
		throw BluetoothException("buffer cannot be null");

	if (length == 0)
		return;

	if (data->s == INVALID_SOCKET)
		throw BluetoothException("Attempting to write to a closed connection");

	if (send(data->s, buffer, length, 0) != length)
		throw BluetoothException("Writing attempt was unsuccessful");
}

bool BTSerialPortBinding::IsDataAvailable()
{
	if (data->s == INVALID_SOCKET)
		throw BluetoothException("connection has been closed");

	u_long count;
	int iResult = ioctlsocket(data->s, FIONREAD, &count);
	if (iResult != NO_ERROR)
	{
		// invalidate the socket so the user can re-open it //
		Close();
		switch (iResult)
		{
		case WSANOTINITIALISED:
			data->initialized = false;
			throw BluetoothException("bluetooth not initialized!");
			break;
		case WSAENETDOWN:
			throw BluetoothException("network subsystem failure!");
			break;
		case WSAEINPROGRESS:
			throw BluetoothException("a blocking call is in progress!");
			break;
		case WSAENOTSOCK:
			throw BluetoothException("socket isn't valid!");
			break;
		case WSAEFAULT:
			throw BluetoothException("CRITICAL: count variable address invalid!");
			break;
		}
	}
	return count > 0;
}
void BTSerialPortBinding::setTimoutRead(long seconds, long microSeconds)
{
	if (timeoutRead)
	{
		delete timeoutRead;
		timeoutRead = nullptr;
	}
	if (seconds < 0 || microSeconds < 0)
	{
		return;
	}
	timeoutRead = new timeval;
	timeoutRead->tv_sec  = seconds;
	timeoutRead->tv_usec = microSeconds;
}
