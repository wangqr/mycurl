#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdlib.h>
#include <stdio.h>
#include <string>

#pragma comment (lib, "Ws2_32.lib")

#define RECVBUF_LEN 65536
char recvbuf[RECVBUF_LEN];

int __cdecl main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	int iResult = 0;
	bool verbose = false;
	int url_i = 0;
	int writeout_i = 0;
	int postdata_i = 0;

	// Parse parameters
	bool parse_fail = false;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0]) {
			if (argv[i][0] == '-') {
				if (argv[i][1] == 'd') {
					i += 1;
					if (i >= argc) {
						parse_fail = true;
						break;
					}
					postdata_i = i;
				}
				else if (argv[i][1] == 'o') {
					i += 1;
					if (i >= argc) {
						parse_fail = true;
						break;
					}
					writeout_i = i;
				}
				else if (argv[i][1] == 'v') {
					verbose = true;
				}
			}
			else {
				if (url_i) {
					parse_fail = true;
					break;
				}
				url_i = i;
			}
		}
	}
	if (parse_fail || !url_i) {
		fputs("Usage: mycurl [options...] <url>\nOptions:\n -d DATA             HTTP POST data\n -o FILE             Write to FILE instead of stdout\n -v                  Make the operation more talkative\n", stderr);
		return -1;
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Parse URL
	// Example: http://wangqr.tk:8080/ping.png
	// host = "wangqr.tk:8080"
	// path = "/ping.png"
	// domain = "wangqr.tk"
	// port = "8080"
	std::string host(argv[url_i]);
	std::string prefix("http://");
	if (!host.compare(0, prefix.size(), prefix))
		host = host.substr(prefix.size());
	size_t sz_pos = host.find('/');
	std::string path;
	if (sz_pos != std::string::npos) {
		path = host.substr(sz_pos);
	}
	else {
		path = "/";
	}
	host = host.substr(0, sz_pos);
	sz_pos = host.rfind(':');
	std::string port, domain;
	if (sz_pos != std::string::npos) {
		port = host.substr(sz_pos + 1);
		domain = host.substr(0, sz_pos);
	}
	else {
		port = "80";
		domain = host;
	}

	// Resolve the server address and port
	iResult = getaddrinfo(domain.c_str(), port.c_str(), &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		if (verbose) {
			const void *paddr = ptr->ai_family == AF_INET ? (void*)&((struct sockaddr_in*)ptr->ai_addr)->sin_addr :
				ptr->ai_family == AF_INET6 ? (void*)&((struct sockaddr_in6*)ptr->ai_addr)->sin6_addr : NULL;
			inet_ntop(ptr->ai_family, paddr, recvbuf, RECVBUF_LEN);
			fputs("*   Trying ", stderr);
			fputs(recvbuf, stderr);
			fputs("...\n", stderr);
		}
		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		if (verbose) {
			fputs("* Connected to ", stderr);
			fputs(domain.c_str(), stderr);
			fputs(" (", stderr);
			fputs(recvbuf, stderr);
			fputs(") port ", stderr);
			fputs(port.c_str(), stderr);
			fputs(" (#0)\n", stderr);
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	// Construct request line
	std::string sendbuf, sendbuf_line(postdata_i ? "POST " : "GET ");
	sendbuf_line.append(path);
	sendbuf_line.append(" HTTP/1.1");
	if (verbose) {
		fputs("> ", stderr);
		fputs(sendbuf_line.c_str(), stderr);
	}
	sendbuf = sendbuf_line;
	sendbuf.append("\r\n");

	// Dirty way to build header
	sendbuf_line = "Host: ";
	sendbuf_line.append(host);
	if (verbose) {
		fputs("\n> ", stderr);
		fputs(sendbuf_line.c_str(), stderr);
	}
	sendbuf.append(sendbuf_line);
	sendbuf.append("\r\n");

	sendbuf_line = "User-Agent: mycurl/1.0";
	if (verbose) {
		fputs("\n> ", stderr);
		fputs(sendbuf_line.c_str(), stderr);
	}
	sendbuf.append(sendbuf_line);
	sendbuf.append("\r\n");

	sendbuf_line = "Accept: */*";
	if (verbose) {
		fputs("\n> ", stderr);
		fputs(sendbuf_line.c_str(), stderr);
	}
	sendbuf.append(sendbuf_line);
	sendbuf.append("\r\n");

	sendbuf_line = "Accept-Encoding: identity";
	if (verbose) {
		fputs("\n> ", stderr);
		fputs(sendbuf_line.c_str(), stderr);
	}
	sendbuf.append(sendbuf_line);
	sendbuf.append("\r\n");

	sendbuf_line = "Connection: close";
	if (verbose) {
		fputs("\n> ", stderr);
		fputs(sendbuf_line.c_str(), stderr);
	}
	sendbuf.append(sendbuf_line);
	sendbuf.append("\r\n");

	if (postdata_i) {
		sendbuf_line = "Content-Length: ";
		sendbuf_line.append(std::to_string(strlen(argv[postdata_i])));
		if (verbose) {
			fputs("\n> ", stderr);
			fputs(sendbuf_line.c_str(), stderr);
		}
		sendbuf.append(sendbuf_line);
		sendbuf.append("\r\n");

		sendbuf_line = "Content-Type: application/x-www-form-urlencoded";
		if (verbose) {
			fputs("\n> ", stderr);
			fputs(sendbuf_line.c_str(), stderr);
		}
		sendbuf.append(sendbuf_line);
		sendbuf.append("\r\n");
	}

	// Header end
	if (verbose) {
		fputs("\n>\n", stderr);
	}
	sendbuf.append("\r\n");

	if (postdata_i) {
		sendbuf.append(argv[postdata_i]);
	}

	iResult = send(ConnectSocket, sendbuf.c_str(), (int)sendbuf.length(), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}

	// Receive until the peer closes the connection
	int is_body = 2;
	FILE* out = writeout_i ? _fsopen(argv[writeout_i], "wbS", _SH_DENYWR) : stdout;
	do {
		iResult = recv(ConnectSocket, recvbuf, RECVBUF_LEN, 0);
		if (iResult > 0) {
			for (size_t i = 0; i < iResult; ++i) {
				if (is_body == 1) {
					i += fwrite(recvbuf + i, 1, iResult - i, out);
				}
				else if (recvbuf[i] == '\r') {
					continue;
				}
				else if (is_body == 2) {
					if (verbose) {
						fputs("< ", stderr);
						fputc(recvbuf[i], stderr);
					}
					is_body = recvbuf[i] == '\n';
					if (is_body) {
						fflush(stderr);
					}
				}
				else {
					if (verbose) {
						fputc(recvbuf[i], stderr);
					}
					is_body = (recvbuf[i] == '\n') << 1;
				}
			}
		}
	} while (iResult > 0);

	// cleanup
	if (out != stdout) {
		fclose(out);
	}
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}
