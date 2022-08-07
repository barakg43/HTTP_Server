#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <fstream>
#include <ostream>
#include <iostream>

using std::queue;
using std::string;
using std::vector;
using std::ifstream;
using std::ostream;

class HttpWebServer
{
	sockaddr_in serverService;
	SOCKET listenSocket;
	const string endOfLine = "\r\n";
	const string rootPath = "C:\\temp\\";
	const string defaultHomePage = "index.html";

	class SocketState
	{
		enum STATE { EMPTY = 0, LISTEN, RECEIVE, IDLE, SEND };
		SOCKET id;			// Socket handle
		int	recv;			// Receiving
		int	send = STATE::IDLE;			// Sending
		time_t timeInTheLastRequest;// Sending sub-type
		
		struct HttpMessageResponse
		{
			string requestName;
			vector<string> msg;
		};

		queue<HttpMessageResponse> respnsQuque;
		vector<string> reqstQuque;
		int len = 0;

		SocketState(SOCKET& socket_id, int _recv, time_t timeOfAddSocket) :id(socket_id), recv(_recv), timeInTheLastRequest(timeOfAddSocket) {}
		friend class HttpWebServer;
	};

	vector<SocketState> socketVector;
	void initServicesServer();

	enum SERVER_OPTIONS { BAD_REQ = 0, OPTIONS, GET, HEAD, POST, PUT, _DELETE, TRACE };
	enum SERVER_SETTING { SRV_PORT = 8080, MAX_SOCKETS = 60, BUFFER_SIZE = 4096, TIMEOUT_CONN_SEC = 120 };
	enum STATUS_CODES { OK = 200, CREATED, NO_CONTENT = 204, BAD_REQUEST = 400, NOT_FOUND = 404, METHOD_NOT_ALLOWED = 405, LENGHT_REQUIRED = 411, INRL_SRV_ERR = 500, NOT_IMPLEMENTED = 501 };

	std::map<short, string> httpStatusResponceMap{
		{STATUS_CODES::OK,"200 OK"},
		{STATUS_CODES::CREATED,"201 Created"},
		{STATUS_CODES::BAD_REQUEST,"400 Bad Request"},
		{STATUS_CODES::NOT_FOUND,"404 Not Found"},
		{STATUS_CODES::METHOD_NOT_ALLOWED,"405 Method Not Allowed"},
		{STATUS_CODES::LENGHT_REQUIRED,"411 Length Required"},
		{STATUS_CODES::INRL_SRV_ERR,"500 Internal Server Error"},
		{STATUS_CODES::NOT_IMPLEMENTED,"501 Not Implemented"},
		{STATUS_CODES::NO_CONTENT, "204 No Content"}};
	
	std::map<string, int> ServerOptionsReqMap{
		{"OPTIONS",SERVER_OPTIONS::OPTIONS},
		{"GET",SERVER_OPTIONS::GET},
		{"HEAD",SERVER_OPTIONS::HEAD},
		{"POST",SERVER_OPTIONS::POST},
		{"PUT",SERVER_OPTIONS::PUT},
		{"DELETE",SERVER_OPTIONS::_DELETE},
		{"TRACE",SERVER_OPTIONS::TRACE} };
	
	bool addSocket(SOCKET id, int what);

	void removeSocket(int index);

	void acceptConnection(int index);

	void receiveMessage(int index);

	void sendMessage(int index);
	
	bool checkIfConnectionSocketArriveTimeout(int indexSocket) { return getCurrentTime() - socketVector[indexSocket].timeInTheLastRequest > SERVER_SETTING::TIMEOUT_CONN_SEC; }
	
	bool checkIfFileExist(const string& fileName)
	{ ifstream file(fileName, std::ios_base::in); 
			return file.good();                   
	}
		  
	time_t getCurrentTime() {
		time_t timer;
		time(&timer);

		return timer;
	}
	
	void  splitRequestByNewline(const string& str, vector<string>& userReqByLine);
	
	void processHTTPMsgRequest(vector<string>& httpReqByLine, int indxSockt);
	
	string CreateHeadersOfResponseForOptions(const string& status);
	
	vector<string> CreateErrorResponse(int statusCode, const string& bodyMsg);
	
	string getFileLocationFromHttpRequest(vector<string>& userFullReq);
	
	long CalculateRequestHeadersLength(vector<string>& userFullReq);
	
	void addHeadersFromRequestAsBody(vector<string>& userFullReq, vector<string>& response);
	
	vector<string> handleGetOrHeadReq(vector<string>& userFullReq, bool withBody, const string& requestName);
	
	vector<string> handleGetReq(vector<string>& userFullReq);
	
	vector<string> handlePutReq(vector<string>& userFullReq);
	
	vector<string> handleTraceReq(vector<string>& userFullReq);

	vector<string> handlePostReq(vector<string>& userFullReq);

	vector<string> handleHeadReq(vector<string>& userFullReq);

	vector<string> handleOptionsReq(vector<string>& userFullReq);

	vector<string> handleDeleteReq(vector<string>& userFullReq);

	void addBody(ifstream& file, vector<string>& response);

	string CreateHeadersOfResponse(const string& status, long contentLength=0,const string& contentType="text/html");

	long CalculateContentLength(ifstream& file);

	std::map<string, string> getHeaderMap(vector<string>& userFullReq, int& endOfHeaderLines, int& headerSizeBytes);

	void outputMsgBody(ostream& os, vector<string>& userFullReq);
	
	string convertNumberTo2Digitit(long num) { return (num < 10) ? '0' + std::to_string(num) : std::to_string(num); }
	
	string getDateAndTimeWithHttpTimeFormatStr();
	
	void getRequestOptionFromMsg(const string& req, int indexSockt);
	
	bool checkRequestValidation(vector<string>& userFullReq, int index, const string& requestName);
	
public:
	
	void start();

	HttpWebServer() {socketVector.reserve(SERVER_SETTING::MAX_SOCKETS);}
};

