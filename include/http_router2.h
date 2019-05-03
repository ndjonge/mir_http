#include <iterator>
#include <map>
#include <memory>
#include <stack>
#include <string>

#include "http_basic.h"

namespace http
{

namespace api
{

using route_function_t2 = std::function<void(http::session_handler& session, const http::api::params& params)>;

template <typename M = http::method::method_t, typename T = std::string, typename R = route_function_t2> class router2
{
public:
	struct metrics
	{
		friend class route_result;

		metrics() = default;
		metrics(const metrics& r) noexcept
		{
			request_latency_.store(r.request_latency_);
			processing_duration_.store(r.processing_duration_);
			hit_count_.store(r.hit_count_);
		}

		metrics& operator=(const metrics& r) noexcept
		{
			request_latency_.store(r.request_latency_);
			processing_duration_.store(r.processing_duration_);
			hit_count_.store(r.hit_count_);
		}

		metrics(metrics&& r) noexcept
		{
			request_latency_.store(r.request_latency_);
			processing_duration_.store(r.processing_duration_);
			hit_count_.store(r.hit_count_);
		}

		metrics& operator=(metrics&& r) noexcept
		{
			request_latency_.store(r.request_latency_);
			processing_duration_.store(r.processing_duration_);
			hit_count_.store(r.hit_count_);
		}

		std::atomic<std::chrono::high_resolution_clock::duration> request_latency_{};
		std::atomic<std::chrono::high_resolution_clock::duration> processing_duration_{};

		std::atomic<std::int64_t> hit_count_{ 0 };

		std::string to_string()
		{
			std::stringstream s;

			s << request_latency_.load().count() / 1000000.0 << "ms, " << processing_duration_.load().count() / 1000000.0 << "ms, " << hit_count_ << "x";

			return s.str();
		};
	};

	class route_part
	{
		friend class router2;

	private:
		std::map<T, std::unique_ptr<route_part>> link_;
		std::map<M, std::unique_ptr<std::tuple<R, metrics>>> endpoints_;
		T key_;

	public:
		route_part(const T& key)
			: key_(key)
		{
		}

		route_part(const T& key, const R& endpoint)
			: key_(key)
			, endpoint_(endpoint)
		{
		}
	};

	std::unique_ptr<route_part> root_;

public:
	struct result
	{
		result(http::router_result::router_result_type result, const R* endpoint, const metrics* m)
			: result_(result)
			, endpoint_(endpoint)
			, metrics_(m)
		{
		}

		http::router_result::router_result_type result_;
		const R* endpoint_;
		const metrics* metrics_;
	};

	router2()
		: root_(new router2::route_part(T()))
	{
	}

	void on_get(const http::method::method_t m, const std::string& r, const R& api_method) { add(m, r, api_method); }

	void add(const M& method, const T& route, const R& end_point)
	{
		auto it = root_.get();

		auto parts = http::util::split(route, "/");

		for (const auto& part : parts)
		{
			auto& l = it->link_[part];

			if (!l)
			{
				l.reset(new route_part(part));
			}

			it = l.get();
		}

		it->endpoints_[method].reset(new std::tuple<R, metrics>(end_point, metrics()));
	}

	void remove(const T& s)
	{
		auto it = root_.get();
		std::stack<decltype(it)> bak;

		for (auto c : s_)
		{
			auto link = it->link_.find(c);
			if (link == std::end(it->link_))
			{
				return; // No match
			}
			bak.push(it);
			it = link_->second.get();
		}

		it->terminal = false;
		if (it->link_.size() == 0)
		{
			T key = it->key_;

			while (!bak.empty() && bak.top()->link.size() _ <= 1 && !bak.top()->terminal)
			{
				key = bak.top()->key_;
				bak.pop();
			}
			if (!bak.empty())
			{
				// Average case: Trim the tail from the parent's links
				bak.top()->link_.erase(key);
			}
			else
			{
				// Edge case: This was the last path in the trie
				root->link_.clear();
			}
		}
	}

	result route(const http::method::method_t& method, const std::string& url, params& params) const noexcept
	{
		auto it = root_.get();
		auto parts = http::util::split(url, "/");

		for (const auto& part : parts)
		{
			auto l = it->link_.find(part);

			if (l == std::end(it->link_))
			{
				// The path was exhausted while searching
				return result{ router_result::no_route, nullptr, nullptr };
			}

			it = l->second.get();
		}

		const auto& endpoint = it->endpoints_.find(method);

		if (endpoint != it->endpoints_.cend())
		{
			auto& z = std::get<0>(*endpoint->second);
			auto& y = std::get<1>(*endpoint->second);

			return result(http::router_result::match_found, nullptr, nullptr);
		}

		return result(http::router_result::match_found, nullptr, nullptr);
	}

	// typename std::basic_string<T>::size_type match(const T& s, bool require_terminal = true) const;
};

} // namespace api

} // namespace http

void test()
{
	http::api::router2<> t;

	t.on_get(http::method::get, "/v1/service1/subservice1/resource1/subresource1", [](http::session_handler& session, const http::api::params& params) { std::cout << "Hoi1!\n"; });
	t.on_get(http::method::put, "/v1/service2/subservice1/resource1/subresource1", [](http::session_handler& session, const http::api::params& params) { std::cout << "Hoi2!\n"; });

	http::api::params p;

	auto route_result = t.route(http::method::get, "/v1/service1/subservice1/resource1/subresource1", p);

	http::configuration c{ {} };
	http::session_handler s(c);

}