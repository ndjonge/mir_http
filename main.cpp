#include "http.h"
#include "server.h"
#include <sstream>
#include <functional>
#include "json.h"


/*int main(int argc, char* argv[])
{

	http::server<http::connection_handler_http, http::connection_handler_https> server(
		"C:\\Development Libraries\\ssl.crt", 
		"C:\\Development Libraries\\ssl.key");

	server.start_server();

	return 0;
}*/


namespace application
{
	namespace routers
	{
		namespace json
		{
			class router
			{
			public:
				router() = default;
			private:
				std::map<const char*, std::function<double(double, double)> > dispTable{
					{ "addRequest",[](double a, double b) { return a + b; } },
					{ "concatRequest",[](double a, double b) { return a - b; } }
				};

			};
		}
	}
}

int main(void)
{
	const char addRequest[] = "{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"id\":0,\"params\":[3,2]}";
	const char concatRequest[] = "{\"jsonrpc\":\"2.0\",\"method\":\"concat\",\"id\":1,\"params\":[\"Hello, \",\"World!\"]}";
	const char addArrayRequest[] = "{\"jsonrpc\":\"2.0\",\"method\":\"add_array\",\"id\":2,\"params\":[[1000,2147483647]]}";
	const char toStructRequest[] = "{\"jsonrpc\":\"2.0\",\"method\":\"to_struct\",\"id\":5,\"params\":[[12,\"foobar\",[12,\"foobar\"]]]}";
	const char printNotificationRequest[] = "{\"jsonrpc\":\"2.0\",\"method\":\"print_notification\",\"params\":[\"This is just a notification, no response expected!\"]}";


	namespace x3 = boost::spirit::x3;

	std::string storage = addRequest;

	std::string::const_iterator iter = storage.begin();
	std::string::const_iterator iter_end = storage.end();

	json::value o;

	auto p = parse(storage.begin(), storage.end(), json::parser::json, o);

	std::stringstream s;
	boost::apply_visitor((json::writer(s)), o);

	boost::apply_visitor((json::rpc::dispatcher(application::routers::json::router())), o);

	auto s2 = s.str();
	std::cout << s2 << std::endl;

	json::value o2;
	p = parse(s2.begin(), s2.end(), json::parser::json, o2);

	return 0;
}

