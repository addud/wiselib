/*
* Simple Wiselib Example
*/
#include "external_interface/external_interface.h"
#include "algorithms/routing/ctp/ctp_routing_engine.h"
#include "algorithms/routing/ctp/ctp_forwarding_engine.h"
#include "algorithms/routing/ctp/ctp_link_estimator.h"
#include "algorithms/routing/ctp/ctp_routing_table_value.h"
#include "algorithms/routing/ctp/ctp_neighbour_table_value.h"
#include "algorithms/routing/ctp/ctp_random_number.h"
#include "algorithms/routing/ctp/ctp_debugging.h"
#include "algorithms/routing/ctp/ctp_types.h"
#include "algorithms/routing/ctp/ctp_send_queue_value.h"
#include "internal_interface/routing_table/routing_table_static_array.h"
#include "util/base_classes/routing_base.h"
#include "util/pstl/queue_static.h"
#include "debug_radio.h"

typedef wiselib::OSMODEL Os;
typedef Os::TxRadio TxRadio;
typedef wiselib::DebugRadio<Os,TxRadio> Radio;

//typedef TxRadio Radio;

typedef Os::Debug Debug;
typedef Os::Clock Clock;
typedef Os::Timer Timer;
typedef Radio::node_id_t node_id_t;
typedef Radio::block_data_t block_data_t;

typedef wiselib::CtpRoutingTableValue<Radio> RoutingTableValue;
//This structure holds route information about a number of neighbours
typedef wiselib::StaticArrayRoutingTable<Os, Radio, 10, RoutingTableValue> RoutingTable;
typedef wiselib::CtpRandomNumber<Os> RandomNumber;

typedef wiselib::CtpNeighbourTableValue<Radio>NeighbourTableValue;
//This structure holds link quality information about a number of neighbours
typedef wiselib::StaticArrayRoutingTable<Os, Radio, 10, NeighbourTableValue> NeighbourTable;
typedef wiselib::CtpLinkEstimator<Os, NeighbourTable, RandomNumber, Radio> LinkEstimator;

//The ECN_ENABLED parameter set to true enables the Explicit Congestion Notification in the RE
//This deals with detecting congestion situations and balancing the traffic accordingly
typedef wiselib::CtpRoutingEngine<Os, LinkEstimator, RoutingTable, RandomNumber,true,
	LinkEstimator> RoutingEngine;

typedef wiselib::CtpForwardingEngineMsg<Os, Radio> DataMessage;
typedef wiselib::CtpSendQueueValue<Radio, DataMessage> SendQueueValue;
//This is used as a buffer for the messages to be sent
typedef wiselib::queue_static<Os, SendQueueValue*, 13> SendQueue;
//This is used as a pool for pre-allocated memory locations for the entries Sendqueue
typedef wiselib::queue_static<Os, SendQueueValue, 13> EntryPool;
//This is used as a pool for pre-allocated memory locations for the messages in the Sendqueue
typedef wiselib::queue_static<Os, DataMessage, 13> MessagePool;
//This cache stores the last few sent messages
typedef wiselib::vector_static<Os, DataMessage*, 4> SentCache;
typedef wiselib::CtpForwardingEngine<Os, DataMessage, SendQueueValue, SendQueue,
	EntryPool, MessagePool, SentCache, RandomNumber, RoutingEngine, LinkEstimator, Radio> ForwardingEngine;

class CtpTest {

public:

	char c;

	void init(Os::AppMainParameter& value) {
		txradio_ = &wiselib::FacetProvider<Os, TxRadio>::get_facet(value);
		timer_ = &wiselib::FacetProvider<Os, Timer>::get_facet(value);
		debug_ = &wiselib::FacetProvider<Os, Debug>::get_facet(value);
		clock_ = &wiselib::FacetProvider<Os, Clock>::get_facet(value);

		txradio_->set_power(TxRadio::TxPower::MAX);

		//		radio_ = txradio_;

		radio_ = &debug_radio_;
		radio_->init(*txradio_);




		random_number_.init(*txradio_, *debug_, *clock_);

		le_.init(*radio_, *timer_, *debug_, *clock_, random_number_);
		le_.enable_radio();

		re_.init(le_, *timer_, *debug_, *clock_, random_number_, le_);
		re_.enable();

		fe_.init(*radio_, *timer_, *debug_, *clock_, random_number_, re_,le_);
		fe_.enable_radio();

		for (int i = 0; i < ROOT_NODES_NR; i++) {
			if (radio_->id() == wiselib::nodes[wiselib::root_nodes[i]]) {
				re_.command_RootControl_setRoot();
				break;
			}
		}

		debug_->debug("Node %x started\n", radio_->id());

		fe_.reg_recv_callback<CtpTest, &CtpTest::receive_radio_message>(this);

		//Start message sending from the senders
		for (int i=0;i<SENDER_NODES_NR;i++) {
			if (radio_->id() == wiselib::nodes[wiselib::sender_nodes[i]]) {
				timer_->set_timer<CtpTest, &CtpTest::start>(5000, this, 0);
			}
		}

		timer_->set_timer<CtpTest, &CtpTest::first_change>(20000, this, 0);

		c='0';
	}
	// --------------------------------------------------------------------

	//Sends periodic messages
	void start(void*) {
		block_data_t message[] = " caca\0";

		message[0] = c++;

		debug_->debug("%x: APP sends message %s\n", radio_->id(), message);

		fe_.send(Radio::BROADCAST_ADDRESS, sizeof(message), message);

		// following can be used for periodic messages to sink
		timer_->set_timer < CtpTest, &CtpTest::start > (5000, this, 0);
	}

	// --------------------------------------------------------------------
	void receive_radio_message(node_id_t from, Radio::size_t len,
		block_data_t *buf) {
			debug_->debug("%x: APP: Message %s reached the root from %x\n",
				radio_->id(), buf, from);
	}

	//Changes in network topology used during tests
	void first_change(void*) {

		debug_->debug("APP: First change occured *****************");

		radio_->change_link(0,1,2,2,MAX_LINK_VALUE);


		timer_->set_timer < CtpTest, &CtpTest::second_change > (30000, this, 0);
	}

	void second_change(void*) {
		debug_->debug("APP: Second change occured *****************");

		radio_->change_link(1,1,0,2,100);

//		for (int i=0;i<LINKS_NR;i++) {
//			debug_->debug("Link[%d]: %d - %d = %d",i, radio_->links[i][0], radio_->links[i][1], radio_->links[i][2]);
//		}

		//timer_->set_timer < CtpTest, &CtpTest::second_change > (20000, this, 0);
	}

private:
	Radio debug_radio_;
	TxRadio::self_pointer_t txradio_;
	Radio::self_pointer_t radio_;
	Timer::self_pointer_t timer_;
	Debug::self_pointer_t debug_;
	Clock::self_pointer_t clock_;
	RandomNumber random_number_;
	LinkEstimator le_;
	RoutingEngine re_;
	ForwardingEngine fe_;
};

// --------------------------------------------------------------------------
wiselib::WiselibApplication<Os, CtpTest> example_app;
// --------------------------------------------------------------------------
void application_main(Os::AppMainParameter& value) {
	example_app.init(value);
}
