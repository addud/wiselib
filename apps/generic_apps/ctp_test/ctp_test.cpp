/*
 * Simple Wiselib Example
 */
#include "external_interface/external_interface.h"
#include "algorithms/routing/ctp/ctp_routing_engine.h"
#include "algorithms/routing/ctp/ctp_forwarding_engine.h"
#include "algorithms/routing/ctp/ctp_link_estimator.h"
#include "algorithms/routing/ctp/ctp_routing_table_value.h"
#include "algorithms/routing/ctp/ctp_random_number.h"
#include "algorithms/routing/ctp/ctp_debugging.h"
#include "algorithms/routing/ctp/ctp_send_queue_value.h"
#include "internal_interface/routing_table/routing_table_static_array.h"
#include "util/base_classes/routing_base.h"
#include "util/pstl/queue_static.h"

typedef wiselib::OSMODEL Os;
typedef wiselib::CtpRoutingTableValue<Os::Radio> RoutingTableValue;
typedef wiselib::StaticArrayRoutingTable<Os, Os::Radio, 10, RoutingTableValue> RoutingTable;
typedef wiselib::CtpRandomNumber<Os, Os::Clock, Os::Debug> RandomNumber;
typedef wiselib::CtpLinkEstimator<Os, RoutingTable, RandomNumber, Os::Radio,
		Os::Timer, Os::Debug, Os::Clock> LinkEstimator;
typedef wiselib::CtpRoutingEngine<Os, RoutingTable, RandomNumber, LinkEstimator> RoutingEngine;

typedef wiselib::CtpSendQueueValue<Os::Radio> SendQueueValue;
typedef wiselib::queue_static<Os, SendQueueValue*, 13> SendQueue;
typedef wiselib::queue_static<Os, SendQueueValue*, 13> QueueEntryPool;
typedef wiselib::vector_static<Os, Os::Radio::block_data_t*, 4> SentCache;
typedef wiselib::CtpForwardingEngine<Os, SendQueueValue, SendQueue, QueueEntryPool, SentCache, RandomNumber, RoutingEngine> ForwardingEngine;
typedef Os::Radio Radio;
typedef Radio::node_id_t node_id_t;

class CtpTest {

public:
	void init(Os::AppMainParameter& value) {
		radio_ = &wiselib::FacetProvider<Os, Os::Radio>::get_facet(value);
		timer_ = &wiselib::FacetProvider<Os, Os::Timer>::get_facet(value);
		debug_ = &wiselib::FacetProvider<Os, Os::Debug>::get_facet(value);
		clock_ = &wiselib::FacetProvider<Os, Os::Clock>::get_facet(value);

//		radio_->set_power(Radio::TxPower::MAX);
		random_number_.init(*debug_, *clock_);

		le_.init(*radio_, *timer_, *debug_, *clock_, random_number_);
		re_.init(le_, *timer_, *debug_, *clock_, random_number_);
		fe_.init(*radio_, *timer_, *debug_, *clock_, random_number_, re_);
		re_.enable_radio();
		fe_.enable_radio();


		for (int i = 0; i < ROOT_NODES_NR; i++) {
			if (radio_->id() == wiselib::nodes[wiselib::root_nodes[i]]) {
				re_.command_RootControl_setRoot();
				break;
			}
		}

		debug_->debug("Node %d started", radio_->id());

		re_.reg_event_callback < CtpTest, &CtpTest::event > (this);
		re_.reg_recv_callback < CtpTest, &CtpTest::receive_radio_message
				> (this);

		if (radio_->id() == wiselib::nodes[3]) {
			timer_->set_timer < CtpTest, &CtpTest::start > (12000, this, 0);
		}
	}
	// --------------------------------------------------------------------
	void start(void*) {
		Os::Radio::block_data_t message[] = "ac\0";
		debug_->debug("%d: APP: sends to %d\n", radio_->id(),
				re_.command_Routing_nextHop());
		re_.send(re_.command_Routing_nextHop(), sizeof(message), message);

		// following can be used for periodic messages to sink
		timer_->set_timer < CtpTest, &CtpTest::start > (5000, this, 0);
	}
	// --------------------------------------------------------------------
	void receive_radio_message(Os::Radio::node_id_t from, Os::Radio::size_t len,
			Os::Radio::block_data_t *buf) {
//		debug_->debug("  message is %s\n", buf);
		if (re_.command_RootControl_isRoot()) {
			debug_->debug("%d: APP Packet reached the root\n", radio_->id());
		} else {
			debug_->debug("%d: APP forwarding from %d to %d\n", radio_->id(),
					from, re_.command_Routing_nextHop());
			re_.send(re_.command_Routing_nextHop(), len, buf);
		}

	}

	void event(uint8_t code) {
//		debug_->debug("CALLBACK: %d\n", code);
	}
private:
	Radio::self_pointer_t radio_;
	Os::Timer::self_pointer_t timer_;
	Os::Debug::self_pointer_t debug_;
	Os::Clock::self_pointer_t clock_;
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
