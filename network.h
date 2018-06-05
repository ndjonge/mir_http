#pragma once



#if defined(_WIN32)
#include <Ws2tcpip.h>
#include <winsock2.h>
#endif

#include <stdio.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace network
{
using socket_t = SOCKET;

void init()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

class buffer
{
public:
	buffer(char* data, size_t size) : data_(data), size_(size)
	{
	}

	char* data(){return data_;}
	size_t size() { 
		return size_;
	}
private:
	char* data_;
	size_t size_;
};

namespace ssl
{

void init()
{
    SSL_load_error_strings();	
    OpenSSL_add_ssl_algorithms();
}

void cleanup()
{
    EVP_cleanup();
}

class context
{
public:

	enum method
	{
		tlsv12
	};

	context(method m)
	{

		switch(m)
		{
			case tlsv12:
				ssl_method_ = TLSv1_2_method();
				break;
		}
		context_ = SSL_CTX_new(ssl_method_);
	}

	void use_certificate_chain_file(const char* path)
	{
		SSL_CTX_set_ecdh_auto(context_, 1);

		/* Set the key and cert */
		if (SSL_CTX_use_certificate_file(context_, path, SSL_FILETYPE_PEM) <= 0) {
			ERR_print_errors_fp(stderr);
			exit(EXIT_FAILURE);
		}

	}

	void use_private_key_file(const char* path)
	{
		if (SSL_CTX_use_PrivateKey_file(context_, path, SSL_FILETYPE_PEM) <= 0 ) {
			ERR_print_errors_fp(stderr);
			exit(EXIT_FAILURE);
		}
	}

	enum verify_mode
	{
		verify_peer,
		verify_fail_if_no_peer_cert,
		verify_client_once
	};

	void set_verify_mode(verify_mode v)//network::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert | boost::asio::ssl::verify_client_once);
	{
		//?
	}

	SSL_CTX* native() { return context_;}

private:
	SSL_CTX* context_;
	const SSL_METHOD* ssl_method_;
};

namespace stream_base
{
	enum handshake_type 
	{
		client,
		server
	};
}

template<class socket>
class stream
{
public:
	stream(context& context) : context_(context), lowest_layer_(0), ssl_(nullptr) 
	{
	}

	~stream()
	{
	}

	void close()
	{
		SSL_free(ssl_);
		ssl_=nullptr;
	}

	const socket& lowest_layer() const
	{
		return lowest_layer_;
	}

	socket& lowest_layer()
	{
		return lowest_layer_;
	}

	SSL* native()
	{
		return ssl_;
	}

	void handshake(stream_base::handshake_type type)
	{
        ssl_ = SSL_new(context_.native());
        SSL_set_fd(ssl_, (int)(lowest_layer_));

        if (SSL_accept(ssl_) <= 0) {
            ERR_print_errors_fp(stderr);
        }
        else 
		{
			SSL_CTX_set_mode(context_.native(), SSL_MODE_AUTO_RETRY);
		}	
	}

private:
	context& context_;
	socket lowest_layer_;
	SSL* ssl_;
};

}



namespace tcp
{

using socket = socket_t;

class endpoint
{
public:
	endpoint() = default;
	virtual void open(std::int16_t protocol) = 0;
	std::int16_t  protocol() {return protocol_;}
	virtual sockaddr* addr()=0;
	virtual int addr_size()=0;
	socket& socket() {return socket_;};

protected:
	tcp::socket  socket_;
	std::int16_t protocol_;
};

/*
class v4 : public endpoint
{
public:
	v4(std::int16_t port) : sock_addr_({})
	{
		protocol_ = SOCK_STREAM;
		sock_addr_.sin_family = AF_INET;
		sock_addr_.sin_port = htons(port);
		sock_addr_.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	sockaddr* addr() {return reinterpret_cast<sockaddr*>(&sock_addr_);};
	std::int32_t addr_size() { return static_cast<std::int32_t>(sizeof(this->sock_addr_));}

	void open(std::int16_t protocol)
	{
		socket_ = ::socket(sock_addr_.sin_family, protocol, 0);
	}
private:
	sockaddr_in sock_addr_;
};
*/

class v6 : public endpoint
{
public:
	v6(std::int16_t port) : sock_addr_({})
	{
		sock_addr_.sin6_family = AF_INET6;
		sock_addr_.sin6_port = ::htons(port);
		sock_addr_.sin6_addr = in6addr_any;
		protocol_ = SOCK_STREAM;
	}

	void open(std::int16_t protocol)
	{
		socket_ = ::socket(sock_addr_.sin6_family, protocol, 0);
	}
	
	sockaddr* addr() {return reinterpret_cast<sockaddr*>(&sock_addr_);};
	std::int32_t addr_size() { return static_cast<std::int32_t>(sizeof(this->sock_addr_));}


private:
	sockaddr_in6 sock_addr_;
};

class acceptor
{
public:
		acceptor() = default;

		void open(std::int16_t protocol) { protocol_ = protocol;}

		void bind(endpoint& endpoint) 
		{
			int ret = 0;
			endpoint_ = &endpoint;
			endpoint_->open(protocol_);
			int reuseaddr = 1;
			ret = ::setsockopt(endpoint_->socket(), SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr));

			DWORD  ipv6only = 0;
			ret = ::setsockopt(endpoint_->socket(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&ipv6only, sizeof(ipv6only));

			ret = ::bind(endpoint_->socket(), endpoint_->addr(), endpoint_->addr_size());

			//ec.value = ret;
		}

		void listen() 
		{
			::listen(endpoint_->socket(), 1);
		}

		void accept(socket& socket) 
		{
			std::int32_t len = static_cast<int>(endpoint_->addr_size());
			socket = ::accept(endpoint_->socket(), endpoint_->addr(), &len);

		}

private:
	std::int16_t protocol_;
	endpoint* endpoint_;
};
}

std::int32_t read(socket_t s, buffer& b)
{
	return ::recv(s, b.data(), static_cast<int>(b.size()), 0);
}

std::int32_t write(socket_t s, buffer& b)
{
	return ::send(s, b.data(), static_cast<int>(b.size()), 0);
}

std::int32_t read(ssl::stream<tcp::socket> s, buffer& b)
{
	return SSL_read(s.native(), b.data(), static_cast<int>(b.size()));
}

std::int32_t write(ssl::stream<tcp::socket> s, buffer& b)
{
	return SSL_write(s.native(), b.data(), static_cast<int>(b.size()));
}

std::string get_client_info(network::ssl::stream<network::tcp::socket>& client_socket)
{
	sockaddr_in6 sa = { 0 };
	socklen_t sl = sizeof(sa);
	char c[INET6_ADDRSTRLEN];

	getpeername(client_socket.lowest_layer(), (sockaddr*)&sa, &sl);

	inet_ntop(AF_INET6, &(sa.sin6_addr), c, INET6_ADDRSTRLEN);

	return c;
}

std::string get_client_info(network::tcp::socket& client_socket)
{
	sockaddr_in6 sa = { 0 };
	socklen_t sl = sizeof(sa);
	char c[INET6_ADDRSTRLEN];

	getpeername(client_socket, (sockaddr*)&sa, &sl);

	inet_ntop(AF_INET6, &(sa.sin6_addr), c, INET6_ADDRSTRLEN);

	return c;
}

void closesocket(network::tcp::socket& client_socket)
{
	::closesocket(client_socket);
}

void closesocket(network::ssl::stream<network::tcp::socket>& client_socket)
{
	client_socket.close();
	::closesocket(client_socket.lowest_layer());
}

void shutdown(network::tcp::socket& client_socket, int how)
{
	::shutdown(client_socket, how);
}

enum shutdown_type
{
	shutdown_receive = 0, shutdown_send, shutdown_both
};

void shutdown(network::ssl::stream<network::tcp::socket>& client_socket, shutdown_type how)
{
	::shutdown(client_socket.lowest_layer(), static_cast<int>(how));
}

}

void test_network()
{
	network::init();

	network::ssl::init();
	network::tcp::v6 endpoint_http{3001};
	network::tcp::v6 endpoint_https{3000};

	network::tcp::acceptor acceptor_http{};
	network::tcp::acceptor acceptor_https{};

	acceptor_http.open(endpoint_http.protocol());
	acceptor_https.open(endpoint_https.protocol());

	acceptor_http.bind(endpoint_http);
	acceptor_https.bind(endpoint_https);

	acceptor_http.listen();
	acceptor_https.listen();

	network::ssl::context ssl_context(network::ssl::context::tlsv12);

	ssl_context.use_certificate_chain_file("C:\\ssl\\server.crt");
	ssl_context.use_private_key_file("C:\\ssl\\server.key");

	network::ssl::stream<network::tcp::socket> https_socket(ssl_context);
	//network::tcp::socket http_socket;

	//std::array<char, 4096> a;

	//acceptor_http.accept(http_socket);
		
	//auto x = network::read(http_socket, network::buffer(a.data(), a.size()));
	//auto y = network::write(http_socket, network::buffer(a.data(), a.size()));

	acceptor_https.accept(https_socket.lowest_layer());
	https_socket.handshake(network::ssl::stream_base::server);


//	auto x2 = network::read(http_socket, network::buffer(a.data(), a.size()));
//	auto y2 = network::write(http_socket, network::buffer(a.data(), a.size()));

	exit(0);

}

