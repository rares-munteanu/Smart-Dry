#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include "mqtt/async_client.h"


class action_listener : public virtual mqtt::iaction_listener
{
	std::string name_;

	void on_failure(const mqtt::token& tok) override {
		std::cout << name_ << " failure";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		std::cout << std::endl;
	}

	void on_success(const mqtt::token& tok) override {
		std::cout << name_ << " success";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		auto top = tok.get_topics();
		if (top && !top->empty())
			std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
		std::cout << std::endl;
	}

public:
	action_listener(const std::string& name) : name_(name) {}
};


class SmartDryHandler : public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
    const int QOS = 1;
    // Counter for the number of connection retries
	int nretry_;
	// The MQTT client
	mqtt::async_client& cli_;
	// Options to use if we need to reconnect
	mqtt::connect_options& connOpts_;
	// An action listener to display the result of actions.
	action_listener subListener_;


    void reconnect() {
		std::this_thread::sleep_for(std::chrono::milliseconds(2500));
		try {
			cli_.connect(connOpts_, nullptr, *this);
		}
		catch (const mqtt::exception& exc) {
			std::cerr << "Error: " << exc.what() << std::endl;
			exit(1);
		}
	}

	// Re-connection failure
	void on_failure(const mqtt::token& tok) override {
		std::cout << "Connection attempt failed" << std::endl;
		if (++nretry_ > 20)
			exit(1);
		reconnect();
	}

    void on_success(const mqtt::token& tok) override {}
    
    void connection_lost(const std::string& cause) override {
		std::cout << "\nConnection lost" << std::endl;
		if (!cause.empty())
			std::cout << "\tcause: " << cause << std::endl;

		std::cout << "Reconnecting..." << std::endl;
		nretry_ = 0;
		reconnect();
	}

    void delivery_complete(mqtt::delivery_token_ptr token) override {}


    void connected(const std::string& cause) override {
		std::cout << "\nConnection success" << std::endl;
		std::cout << "\nSubscribing to topic '" << std::endl;

		cli_.subscribe("smart_dry/initialize", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/start", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/status", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/set_clothes", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/delete_clothes", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/get_programs", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/set_program", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/set_program_status/", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/set_heat", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/set_rpm", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/set_duration", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/stats", QOS, nullptr, subListener_);
        cli_.subscribe("smart_dry/set_perfume", QOS, nullptr, subListener_);
	}

    void message_arrived(mqtt::const_message_ptr msg) override {
		std::cout << "Message arrived" << std::endl;
		std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
		std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
	}


public:

    explicit SmartDryHandler(mqtt::async_client& cli, mqtt::connect_options& connOpts)
				: nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}

    void publish(const std::string &topic, const std::string& payload)
    {
        const auto TIMEOUT = std::chrono::seconds(10);
        const int QOS = 1;
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(QOS);
        cli_.publish(msg)->wait_for(TIMEOUT);
    }
};

int main()
{
    const std::string addr {"tcp://localhost:1883"};
    const std::string clientId {"smart-dry-X033jh-2"};

    mqtt::async_client cli(addr, clientId);

	mqtt::connect_options connOpts;
	connOpts.set_clean_session(false);

	// Install the callback(s) before connecting.
	SmartDryHandler cb(cli, connOpts);
	cli.set_callback(cb);

    try {
		std::cout << "Connecting to the MQTT server..." << std::flush;
		cli.connect(connOpts, nullptr, cb);
	}
	catch (const mqtt::exception& exc) {
		std::cerr << "\nERROR: Unable to connect to MQTT server: '"
			<< addr << "'" << exc << std::endl;
		return 1;
	}

    // Just block till user tells us to quit.

	while (std::tolower(std::cin.get()) != 'q')
		;

	// Disconnect

	try {
		std::cout << "\nDisconnecting from the MQTT server..." << std::flush;
		cli.disconnect()->wait();
		std::cout << "OK" << std::endl;
	}
	catch (const mqtt::exception& exc) {
		std::cerr << exc << std::endl;
		return 1;
	}

    // SmartDryHandler sdHandler(addr);
    // sdHandler.start();
}