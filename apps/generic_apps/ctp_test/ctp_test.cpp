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
#include "algorithms/routing/ctp/ctp_send_queue_value.h"
#include "internal_interface/routing_table/routing_table_static_array.h"
#include "util/base_classes/routing_base.h"
#include "util/pstl/queue_static.h"

typedef wiselib::OSMODEL Os;
typedef Os::TxRadio Radio;
typedef Os::Debug Debug;
typedef Os::Clock Clock;
typedef Os::Timer Timer;
typedef Os::Uart Uart;
typedef Radio::node_id_t node_id_t;
typedef Radio::block_data_t block_data_t;

typedef wiselib::CtpRoutingTableValue<Radio> RoutingTableValue;
typedef wiselib::StaticArrayRoutingTable<Os, Radio, 10, RoutingTableValue> RoutingTable;
typedef wiselib::CtpRandomNumber<Os> RandomNumber;


typedef wiselib::CtpNeighbourTableValue<Radio>NeighbourTableValue;
typedef wiselib::StaticArrayRoutingTable<Os, Radio, 10, NeighbourTableValue> NeighbourTable;
typedef wiselib::CtpLinkEstimator<Os, NeighbourTable, RandomNumber, Radio> LinkEstimator;

typedef wiselib::CtpRoutingEngine<Os, LinkEstimator, RoutingTable, RandomNumber,
		LinkEstimator> RoutingEngine;

typedef wiselib::CtpForwardingEngineMsg<Os, Radio> DataMessage;
typedef wiselib::CtpSendQueueValue<Radio, DataMessage> SendQueueValue;
typedef wiselib::queue_static<Os, SendQueueValue*, 13> SendQueue;
typedef wiselib::queue_static<Os, SendQueueValue, 13> EntryPool;
typedef wiselib::queue_static<Os, DataMessage, 13> MessagePool;
//TODO: Check sentCache for pointers to local variables
typedef wiselib::vector_static<Os, DataMessage*, 4> SentCache;
typedef wiselib::CtpForwardingEngine<Os, DataMessage, SendQueueValue, SendQueue,
		EntryPool, MessagePool, SentCache, RandomNumber, RoutingEngine, Radio> ForwardingEngine;

class CtpTest {

public:

	char c;

	void init(Os::AppMainParameter& value) {
		radio_ = &wiselib::FacetProvider<Os, Radio>::get_facet(value);
		timer_ = &wiselib::FacetProvider<Os, Timer>::get_facet(value);
		debug_ = &wiselib::FacetProvider<Os, Debug>::get_facet(value);
		clock_ = &wiselib::FacetProvider<Os, Clock>::get_facet(value);
		uart_ = &wiselib::FacetProvider<Os, Uart>::get_facet( value );

		radio_->set_power(Radio::TxPower::MAX);
		random_number_.init(*radio_, *debug_, *clock_);

		  uart_->enable_serial_comm();

		  //uart_->reg_read_callback<CtpTest, &CtpTest::receive_serial>( this );


		//TODO: Send random_number_ by value
		le_.init(*radio_, *timer_, *debug_, *clock_, random_number_);
		le_.enable_radio();

		re_.init(le_, *timer_, *debug_, *clock_, random_number_, le_);
		re_.enable();

		fe_.init(*radio_, *timer_, *debug_, *clock_, random_number_, re_);
		fe_.enable_radio();

		for (int i = 0; i < ROOT_NODES_NR; i++) {
			if (radio_->id() == wiselib::nodes[wiselib::root_nodes[i]]) {
				re_.command_RootControl_setRoot();
				break;
			}
		}

		debug_->debug("Node %d started\n", radio_->id());

		fe_.reg_recv_callback<CtpTest, &CtpTest::receive_radio_message>(this);

		if (radio_->id() == wiselib::nodes[3]) {
			timer_->set_timer<CtpTest, &CtpTest::start>(12000, this, 0);
		}

		timer_->set_timer<CtpTest, &CtpTest::change_link>(30000, this, 0);

		c='0';
	}
	// --------------------------------------------------------------------
	void start(void*) {
		block_data_t message[] = " caca\0";

		message[0] = c++;

		//TODO: FiX: doesn't get back here after sending

		debug_->debug("%d: APP sends message %s\n", radio_->id(), message);

		fe_.send(Radio::BROADCAST_ADDRESS, sizeof(message), message);

// following can be used for periodic messages to sink
		timer_->set_timer < CtpTest, &CtpTest::start > (5000, this, 0);
	}

	void change_link(void*) {
		debug_->debug("Removing link\n");
		wiselib::connections[0].etx=1000;
	}

	// --------------------------------------------------------------------
	void receive_radio_message(node_id_t from, Radio::size_t len,
			block_data_t *buf) {
		debug_->debug("%d: APP: Message %s reached the root from %d\n",
				radio_->id(), buf, from);
	}

   void receive_serial( size_t len, block_data_t *buf )
   {
      debug_->debug( "UART: received len %i, message is '%s'\n", len, buf );
      //uart_->write( len, (Uart::block_data_t*)buf );
   }

private:
	Radio::self_pointer_t radio_;
	Timer::self_pointer_t timer_;
	Debug::self_pointer_t debug_;
	Clock::self_pointer_t clock_;
	Uart::self_pointer_t uart_;
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
