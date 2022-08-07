#define _CRT_SECURE_NO_WARNINGS
#include <fstream>
#include "HttpWebServer.h"
#include <iostream>
#include <map>
#include <queue>
#include <time.h>
using std::ofstream;
using std::queue;
using std::stoi;
using std::cout;
using std::endl;
void HttpWebServer::initServicesServer()
{
	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	
	WSAData wsaData;
	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		throw ("HTTP Server: Error at WSAStartup()\n");
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.
	// After initialization, a SOCKET object is ready to be instantiated.
	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	
	if (INVALID_SOCKET == listenSocket)
	{
		cout << getDateAndTimeWithHttpTimeFormatStr()<< " @HTTP Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.
	// Need to assemble the required data for connection in sockaddr structure.
	// Create a sockaddr_in object called serverService. 

	sockaddr_in serverService;

	// Address family (must be AF_INET - Internet address family).
	
	serverService.sin_family = AF_INET;

	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.

	serverService.sin_addr.s_addr = INADDR_ANY;

	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).

	serverService.sin_port = htons(SERVER_SETTING::SRV_PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).

	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.

	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	addSocket(listenSocket, SocketState::STATE::LISTEN);
}

bool HttpWebServer::addSocket(SOCKET id, int what)
{
	if (socketVector.size() != SERVER_SETTING::MAX_SOCKETS)
	{
		socketVector.push_back({ id,what,getCurrentTime() });
		return (true);
	}

	return (false);
}

void HttpWebServer::removeSocket(int index)
{
	socketVector.erase(socketVector.begin()+index);
}

void HttpWebServer::acceptConnection(int index)
{
	SOCKET id = socketVector[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}

	cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	// Set the socket to be in non-blocking mode.
	
	unsigned long flag = 1;

	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, SocketState::STATE::RECEIVE) == false)
	{
		cout << getDateAndTimeWithHttpTimeFormatStr()<< " @HTTP Server:\t\tToo many connections, dropped!\n";
		closesocket(id);
	}

	return;
}

void HttpWebServer::receiveMessage(int index)
{
	SOCKET msgSocket = socketVector[index].id;
	char buffer[SERVER_SETTING::BUFFER_SIZE + 1] = { 0 };//save space for '\0'
	cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: Start Receiving HTTP Request..." << endl;
	int bytesRecv = recv(msgSocket, buffer, SERVER_SETTING::BUFFER_SIZE, 0);
	
	if (SOCKET_ERROR == bytesRecv|| bytesRecv == 0)
	{	
		if (SOCKET_ERROR == bytesRecv)
		{
			cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: Error at recv(): " << WSAGetLastError() << endl;
		}
		
		closesocket(msgSocket);
		removeSocket(index);

		return;
	}

	else
	{
		buffer[bytesRecv] = '\0';
		socketVector[index].timeInTheLastRequest = getCurrentTime();
		vector<string> recvMsg;
		splitRequestByNewline(buffer, recvMsg);
		int endOfHeaderLines, headerSizeBytes;
		std::map<string, string>&& headerMap = getHeaderMap(recvMsg, endOfHeaderLines, headerSizeBytes);
		headerSizeBytes += recvMsg[0].size();
		int isExistContLen = headerMap.count("Content-Length");
		int sumByteRecv = bytesRecv;

		if (isExistContLen) 
		{
			int bodySizeLeft = stoi(headerMap["Content-Length"])- (bytesRecv-headerSizeBytes)+2;//+2 for '\r\n' after header that not include in header size
			while (bodySizeLeft > 0)
			{
				if(bodySizeLeft > SERVER_SETTING::BUFFER_SIZE)
					bytesRecv = recv(msgSocket, buffer, SERVER_SETTING::BUFFER_SIZE, 0);
				else	
					bytesRecv = recv(msgSocket, buffer, bodySizeLeft, 0);

				sumByteRecv += bytesRecv;
				if (SOCKET_ERROR == bytesRecv || bytesRecv == 0)
				{
					if (SOCKET_ERROR == bytesRecv)
						cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: Error at recv(): " << WSAGetLastError() << endl;

					closesocket(msgSocket);
					removeSocket(index);
					return;
				}
				buffer[bytesRecv] = '\0';
				socketVector[index].timeInTheLastRequest = getCurrentTime();
				string bufferStr(buffer);
				int sizeNewLine = bufferStr.find(endOfLine);
				string&& firstLineOfCurrBuffer = bufferStr.substr(0, sizeNewLine);
				string& lastLineInMsg = recvMsg[recvMsg.size() - 1];
				lastLineInMsg.erase(lastLineInMsg.size() - 1, 1);//remove '\n' from middle of line
				lastLineInMsg += firstLineOfCurrBuffer;
				bufferStr.erase(0, sizeNewLine);
				splitRequestByNewline(bufferStr, recvMsg);
				bodySizeLeft -= bytesRecv;
				
			}
		}
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: Received: " << sumByteRecv << "\\" << sumByteRecv << " bytes of '" << recvMsg[0].substr(0, recvMsg[0].find(' ')) << "' request.\n";
		socketVector[index].send = SocketState::STATE::SEND;
		processHTTPMsgRequest(recvMsg, index);
	}

}

void HttpWebServer::outputMsgBody(ostream& os, vector<string>& userFullReq)
{
	int endOfHeaderLines;
	int headerSize;
	auto headerMap = getHeaderMap(userFullReq, endOfHeaderLines, headerSize);

	for (int ind = endOfHeaderLines+1; ind < userFullReq.size(); ind++)
	{
		//remove '\r' form end of each line before print
		if (userFullReq[ind].size() >= 2 && userFullReq[ind][userFullReq[ind].size() - 2] == '\r')
		{
			userFullReq[ind].erase(userFullReq[ind].size() - 2, 1);
		}
		
		os << userFullReq[ind];
	}
}

std::map<string, string> HttpWebServer::getHeaderMap(vector<string>& userFullReq, int& endOfHeaderLines, int& headerSizeBytes)
{
	std::map<string, string> headerMap;
	string header, value;
	int i;

	headerSizeBytes = 0;
	for (i = 1; i < userFullReq.size() && userFullReq[i] != endOfLine; i++)
	{
		auto headerTypeIndex = userFullReq[i].find(':');
		header = userFullReq[i].substr(0, headerTypeIndex);
		value = userFullReq[i].substr(headerTypeIndex + 1, userFullReq[i].length() - headerTypeIndex);
		headerMap.insert({ header,value });
		headerSizeBytes += userFullReq[i].size();
	}

	endOfHeaderLines = i;

	return headerMap;
}

void HttpWebServer::sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[SERVER_SETTING::BUFFER_SIZE];
	SOCKET msgSocket = socketVector[index].id;
	long byteSended = 0;

	if (!socketVector[index].respnsQuque.empty())
	{
		
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: 	Start Sending HTTP Response..." << endl;
		SocketState::HttpMessageResponse& response = socketVector[index].respnsQuque.front();

		int currBufferSize = 0;

		for (int i = 0; i < response.msg.size(); i++)
		{
			if (currBufferSize != 0 && currBufferSize == SERVER_SETTING::BUFFER_SIZE)
			{
				bytesSent = send(msgSocket, sendBuff, currBufferSize, 0);
				byteSended += bytesSent;
				if (SOCKET_ERROR == bytesSent)
				{
					cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: Error at send(): " << WSAGetLastError() << endl;
					
					return;
				}

				currBufferSize = 0;
			}

			int sizeLeftInBuffer = SERVER_SETTING::BUFFER_SIZE - currBufferSize;

			//fill the buffer with pratial line on the Message
			if (response.msg[i].size() > sizeLeftInBuffer)
			{
				memcpy(sendBuff + currBufferSize, response.msg[i].c_str(), sizeLeftInBuffer);
				currBufferSize += sizeLeftInBuffer;
				response.msg[i].erase(0, sizeLeftInBuffer);
				i--;
			}
			else//the line is smaller or equal then current left size in the buffer
			{
				memcpy(sendBuff + currBufferSize, response.msg[i].c_str(), response.msg[i].size());
				currBufferSize += response.msg[i].size();
			}
		}

		if (currBufferSize > 0)
		{
			bytesSent = send(msgSocket, sendBuff, currBufferSize, 0);
			byteSended += bytesSent;
			
		}

		if (SOCKET_ERROR == bytesSent)
		{
			cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: Error at send(): " << WSAGetLastError() << endl;
			
			return;
		}

		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:: Socket ID: " << socketVector[index].id << "#: Sent: " << bytesSent << "\\" << bytesSent << " bytes of '" << response.requestName << "' response.\n";
		socketVector[index].respnsQuque.pop();
	}
	else
	{
		socketVector[index].send = SocketState::STATE::IDLE;
	}
}

void HttpWebServer::start()
{
	initServicesServer();
	cout << getDateAndTimeWithHttpTimeFormatStr()<< ": 	HTTP Server: Wait for clients' requests." << endl;
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < socketVector.size(); i++)
		{
			if ((socketVector[i].recv == SocketState::STATE::LISTEN) || (socketVector[i].recv == SocketState::STATE::RECEIVE))
				FD_SET(socketVector[i].id, &waitRecv);
		}

		for (int i = 0; i < socketVector.size(); i++)
		{
			if (socketVector[i].recv != SocketState::STATE::LISTEN && checkIfConnectionSocketArriveTimeout(i))
			{
				cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: Socket ID:" << socketVector[i].id << "#:close socket timeout.." << endl;
				if (FD_ISSET(socketVector[i].id, &waitRecv)) 
				{
					FD_CLR(socketVector[i].id, &waitRecv);
				}
		
				closesocket(socketVector[i].id);
				removeSocket(i);
			}
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < socketVector.size(); i++)
		{
			if (socketVector[i].send == SocketState::STATE::SEND)
				FD_SET(socketVector[i].id, &waitSend);
		}

		//
		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.
		//
		int NumberOfActiveSocket= select(0, &waitRecv, &waitSend, NULL, NULL);

		if (NumberOfActiveSocket == SOCKET_ERROR)
		{
			cout << getDateAndTimeWithHttpTimeFormatStr()<< ": 	HTTP Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < socketVector.size() && NumberOfActiveSocket > 0; i++)
		{
			if (FD_ISSET(socketVector[i].id, &waitRecv))
			{
				NumberOfActiveSocket--;
				switch (socketVector[i].recv)
				{
				case  SocketState::STATE::LISTEN:
					acceptConnection(i);
					break;

				case  SocketState::STATE::RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < socketVector.size() && NumberOfActiveSocket > 0; i++)
		{
			if (FD_ISSET(socketVector[i].id, &waitSend))
			{
				NumberOfActiveSocket--;
				if(socketVector[i].send== SocketState::STATE::SEND)
					sendMessage(i);
			}
		}
	}

	// Closing connections and Winsock.
	cout << getDateAndTimeWithHttpTimeFormatStr()<< ": 	HTTP Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();


}

void HttpWebServer::processHTTPMsgRequest(vector<string>& httpReqByLine,int indxSockt)
{
	//string tempStr;
	//if(false&&!socketVector[indexSockt].reqstQuque.empty())
	//	string& req1 = socketVector[indexSockt].reqstQuque.front();
	//vector<string> userFullReq;
	//splitRequsetByNewline(req, userFullReq);
	const string& requestHTTPname = httpReqByLine[0].substr(0, httpReqByLine[0].find(' '));
	int optionNum = ServerOptionsReqMap[requestHTTPname];
	int responseCode;

	if (!checkRequestValidation(httpReqByLine,indxSockt, requestHTTPname))
	{
		return;
	}
	switch (optionNum)
	{
	case SERVER_OPTIONS::GET:
		socketVector[indxSockt].respnsQuque.push({ requestHTTPname, handleGetReq(httpReqByLine)});
		break;
	case SERVER_OPTIONS::POST:
		socketVector[indxSockt].respnsQuque.push({ requestHTTPname,handlePostReq(httpReqByLine)});
		break;

	case SERVER_OPTIONS::HEAD:
		socketVector[indxSockt].respnsQuque.push({ requestHTTPname, handleHeadReq(httpReqByLine)});
		break;
	case SERVER_OPTIONS::OPTIONS:
		socketVector[indxSockt].respnsQuque.push({ requestHTTPname, handleOptionsReq(httpReqByLine) });
		break;
	case SERVER_OPTIONS::PUT:
		socketVector[indxSockt].respnsQuque.push({ requestHTTPname,handlePutReq(httpReqByLine) });
		break;
	case SERVER_OPTIONS::_DELETE:
		socketVector[indxSockt].respnsQuque.push({ requestHTTPname,handleDeleteReq(httpReqByLine) });
		break;
	case SERVER_OPTIONS::TRACE:
		socketVector[indxSockt].respnsQuque.push({ requestHTTPname, handleTraceReq(httpReqByLine)});
		break;
	case SERVER_OPTIONS::BAD_REQ:
	default:
		const vector<string>& response = CreateErrorResponse(STATUS_CODES::NOT_IMPLEMENTED, requestHTTPname + " is NOT IMPLEMENTED in this Http Server");
		socketVector[indxSockt].respnsQuque.push({ "NOT IMPLEMENTED",response });
		break;
	}
}

void HttpWebServer::splitRequestByNewline(const string& str, vector<string>& userReqByLine)
{
	std::stringstream ss(str);
	string token;

	while (std::getline(ss, token))
	{
		userReqByLine.push_back(token+'\n');
	}
}

vector<string> HttpWebServer::handleGetOrHeadReq(vector<string>& userFullReq, bool withBody,const string& requestName) {
	vector<string> response;

	string&& fileLocation = getFileLocationFromHttpRequest(userFullReq);
	ifstream file(fileLocation);
	if (!file.good())
	{
		cout << getDateAndTimeWithHttpTimeFormatStr()<< " @HTTP Server: '" << requestName << "':Error opening file at " << fileLocation << endl;
		response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::NOT_FOUND]));
	}
	else
	{
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: '" << requestName << "':opening file " + fileLocation << endl;
		
		long fileLength = CalculateContentLength(file);

		if (withBody) {
			response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::OK], fileLength));
			addBody(file, response);
		}
		else 
		{
			response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::NO_CONTENT], fileLength));
		}	
	}

	file.close();

	return response;
}

vector<string> HttpWebServer::handleGetReq(vector<string>& userFullReq)
{
	return handleGetOrHeadReq(userFullReq, true,"GET");
}

vector<string> HttpWebServer::handleHeadReq(vector<string>& userFullReq)
{
	return handleGetOrHeadReq(userFullReq, false,"HEAD");
}

string HttpWebServer::getDateAndTimeWithHttpTimeFormatStr()
{
	const string shortDayNamesEnglish[] = { "Sun","Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	const string shortMonthNamesEnglish[] = { "Jan", "Feb", "Mar", "Apr","May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	time_t timer = getCurrentTime();
	struct tm* utcTime;

	utcTime = gmtime(&timer);
	//format example :: Sun, 06 Nov 1994 08:49:37 GMT
	string timeString = shortDayNamesEnglish[utcTime->tm_wday] + ", " + convertNumberTo2Digitit(utcTime->tm_mday) + ' ';

	timeString += shortMonthNamesEnglish[utcTime->tm_mon] + ' ' + std::to_string(utcTime->tm_year + 1900) + ' ';
	timeString += convertNumberTo2Digitit(utcTime->tm_hour) + ':' + convertNumberTo2Digitit(utcTime->tm_min) + ':' + convertNumberTo2Digitit(utcTime->tm_sec)+" GMT";
	return timeString;
}

vector<string> HttpWebServer::handlePutReq(vector<string>& userFullReq)		//new
{
	vector<string> response;
	string&& fileLocation = getFileLocationFromHttpRequest(userFullReq);
	bool resourceAlreadyExist = checkIfFileExist(fileLocation);

	ofstream file(fileLocation);
	if (!file.good())
	{
		cout << getDateAndTimeWithHttpTimeFormatStr()<< " @HTTP Server: 'PUT' Request:Error opening file at " + fileLocation << endl;
		response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::INRL_SRV_ERR], 0));
	}
	else
	{
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server: 'PUT' Request: opening file at " + fileLocation << endl;
		outputMsgBody(file, userFullReq);	//writeToFile()
		if(resourceAlreadyExist)
			response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::OK], 0));
		else
			response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::CREATED], 0));
	}

	file.close();

	return response;
}

vector<string> HttpWebServer::handleOptionsReq(vector<string>& userFullReq)
{
	return { CreateHeadersOfResponseForOptions(httpStatusResponceMap[STATUS_CODES::OK]) };
}

vector<string> HttpWebServer::handleDeleteReq(vector<string>& userFullReq)
{
	vector<string> response;
	size_t pos1 = userFullReq[0].find(' ');
	size_t pos2 = userFullReq[0].find(' ', pos1 + 1);
	string&& fileLocation = getFileLocationFromHttpRequest(userFullReq);
	int deleteStatus = remove(fileLocation.c_str());

	if (deleteStatus==0)
	{
		cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:'DELETE' Request:opening file" + fileLocation << endl;
		response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::OK], 0));
	}
	else
	{
		cout << getDateAndTimeWithHttpTimeFormatStr()<<" @HTTP Server:'DELETE' Request:Error opening file at " + fileLocation << endl;
		response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::NOT_FOUND], 0));
	}

	return response;
}

vector<string> HttpWebServer::handleTraceReq(vector<string>& userFullReq)
{
	vector<string> response;
	int requestHeadersLength= CalculateRequestHeadersLength(userFullReq);

	response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::OK], requestHeadersLength,"message/http"));
	addHeadersFromRequestAsBody(userFullReq, response);
	
	return response;
}

vector<string> HttpWebServer::handlePostReq(vector<string>& userFullReq)
{
	vector<string> response;

	cout << getDateAndTimeWithHttpTimeFormatStr() << " @HTTP Server:'POST' Request:			output to STDOUT:" << endl;
	outputMsgBody(cout, userFullReq);	//writeToScreen()
	response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[STATUS_CODES::OK], 0));
	cout << endl;

	return response;
}

void HttpWebServer::addBody(ifstream& file, vector<string>& response)
{
	if (file.is_open())
	{
		file.clear();
		file.seekg(0);
		string currentLineFromFile;

		getline(file, currentLineFromFile);
		while (!file.eof())
		{
			response.push_back(currentLineFromFile+endOfLine);
			getline(file, currentLineFromFile);
		}
	}

	response.push_back(endOfLine);
}

string HttpWebServer::CreateHeadersOfResponse(const string& status, long contentLength, const string& contentType)
{
	string headers = "HTTP/1.1 " + status + endOfLine;

	headers += "Date:" + getDateAndTimeWithHttpTimeFormatStr() + endOfLine;
	headers += "Server:HTTP web server" + endOfLine;
	headers += "Content-Length:" + std::to_string(contentLength) + endOfLine;
	headers += "Content-Type:" + contentType + ";charset=utf-8" + endOfLine;
	headers += "Connection:keep-alive " + endOfLine + endOfLine;
	
	return headers;
}

string HttpWebServer::CreateHeadersOfResponseForOptions(const string& status)
{
	string headers = "HTTP/1.1 " + status + endOfLine, currTime;

	headers += "Date: " + getDateAndTimeWithHttpTimeFormatStr() + endOfLine;
	headers += "Server: T&B HTTP Web Server" + endOfLine;
	headers += "Content-Length: 0" + endOfLine;
	headers += "Content-Type:text/html" + endOfLine;
	headers += "Allow: GET,HEAD,POST,PUT,DELETE,OPTIONS,TRACE " + endOfLine;
	headers += "Accept-Language: en, fr, he, it " + endOfLine + endOfLine;

	return headers;
}

long HttpWebServer::CalculateContentLength(ifstream& file)
{
	long size = 0;

	if (file.is_open()) 
	{
		auto start = file.tellg();
		file.seekg(0, std::ios::end);
		size = file.tellg() - start;
	}

	return size;
}

vector<string> HttpWebServer::CreateErrorResponse(int statusCode, const string& bodyMsg)
{
	vector<string> response;
	string bodyOfRespone = "<!DOCTYPE html>\n<html>\n<body>\n"
							"<p>" + bodyMsg + "</p>\n"
							"</body>\n</html>\n";

	response.push_back(CreateHeadersOfResponse(httpStatusResponceMap[statusCode], bodyOfRespone.size()));
	response.push_back(bodyOfRespone);

	return response;
}

string HttpWebServer::getFileLocationFromHttpRequest(vector<string>& userFullReq)
{
	size_t pos1 = userFullReq[0].find(' ');
	size_t pos2 = userFullReq[0].find(' ', pos1 + 1);
	string fileLocation = userFullReq[0].substr(pos1 + 2, pos2 - pos1 - 2);
	string onlyFileName = fileLocation.substr(0, fileLocation.find('?'));
	size_t requiredLangPosition = fileLocation.find("lang=");
	if (onlyFileName == "")
		onlyFileName = defaultHomePage;
	if (requiredLangPosition < fileLocation.size())
	{
		string requiredLang = fileLocation.substr(requiredLangPosition + 5);

		size_t lastPoint = onlyFileName.find_last_of('.');
		
			string correctFileName = onlyFileName.substr(0, lastPoint) + "_" + requiredLang + onlyFileName.substr(lastPoint);
			onlyFileName = correctFileName;
		
	}

	fileLocation = rootPath + onlyFileName;
	
	return fileLocation;
}

void HttpWebServer::addHeadersFromRequestAsBody(vector<string>& userFullReq, vector<string>& response)
{
	for (int i = 0; i < userFullReq.size() && userFullReq[i] != endOfLine; i++)
	{
		response.push_back(userFullReq[i]);
	}
}

long HttpWebServer::CalculateRequestHeadersLength(vector<string>& userFullReq)
{
	long size = 0;

	for (int i = 0; i < userFullReq.size() && userFullReq[i] != endOfLine; i++)
	{
		size += userFullReq[i].size();
	}

	return size;
}

bool HttpWebServer::checkRequestValidation(vector<string>& userFullReq, int index, const string& requestName)
{
	int endOfHeaders,sizeOfHeaders;
	bool isVaild = true;
	std::map<string, string>&& headersMap=getHeaderMap(userFullReq, endOfHeaders,sizeOfHeaders);
	string httpVer = userFullReq[0].substr(userFullReq[0].find_last_of(' ') + 1);

	if (httpVer == "HTTP/1.1\r\n" && headersMap.count("Host") == 0)
	{
		socketVector[index].respnsQuque.push({ requestName,CreateErrorResponse(STATUS_CODES::BAD_REQUEST,"missing 'Host' header") });
		isVaild = false;
	}
	else if (requestName == "OPTIONS" && endOfHeaders + 1 < userFullReq.size() && headersMap.count("Content-Type") == 0)
	{

		socketVector[index].respnsQuque.push({ requestName,CreateErrorResponse(STATUS_CODES::BAD_REQUEST,"missing 'Content-Type' header for body") });
		isVaild = false;
	}
	else if (requestName == "TRACE" && endOfHeaders + 1 < userFullReq.size())
	{
		socketVector[index].respnsQuque.push({ requestName,CreateErrorResponse(STATUS_CODES::BAD_REQUEST,"trace cannot include body") });
		isVaild = false;
	}
	else if ((requestName == "PUT"|| requestName == "POST") && headersMap.count("Content-Length") == 0)
	{
		socketVector[index].respnsQuque.push({ requestName,CreateErrorResponse(STATUS_CODES::LENGHT_REQUIRED,requestName+" must include 'Content-Length' header") });
		isVaild = false;
	}

	return isVaild;
}
