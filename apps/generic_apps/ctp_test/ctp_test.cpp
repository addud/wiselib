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
#include "util/base_classes/uart_base.h"
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
		radio_ = &wiselib::FacetProvider<Os, Radio>::get_facet(value);
		timer_ = &wiselib::FacetProvider<Os, Timer>::get_facet(value);
		debug_ = &wiselib::FacetProvider<Os, Debug>::get_facet(value);
		clock_ = &wiselib::FacetProvider<Os, Clock>::get_facet(value);
		uart_ = &wiselib::FacetProvider<Os, Uart>::get_facet( value );

		radio_->set_power(Radio::TxPower::MAX);
		random_number_.init(*radio_, *debug_, *clock_);

		uart_->enable_serial_comm();

		uart_->reg_read_callback<CtpTest, &CtpTest::receive_serial>( this );

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

		debug_->debug("Node %d started\n", radio_->id());

		fe_.reg_recv_callback<CtpTest, &CtpTest::receive_radio_message>(this);

		//Start message sending from the senders
		for (int i=0;i<SENDER_NODES_NR;i++) {
			if (radio_->id() == wiselib::nodes[wiselib::sender_nodes[i]]) {
				timer_->set_timer<CtpTest, &CtpTest::start>(5000, this, 0);
			}
		}

		//timer_->set_timer<CtpTest, &CtpTest::first_change>(20000, this, 0);

		c='0';
	}
	// --------------------------------------------------------------------

	//Sends periodic messages
	void start(void*) {
		block_data_t message[] = " caca\0";

		message[0] = c++;

		debug_->debug("%d: APP sends message %s\n", radio_->id(), message);

		fe_.send(Radio::BROADCAST_ADDRESS, sizeof(message), message);

		// following can be used for periodic messages to sink
		timer_->set_timer < CtpTest, &CtpTest::start > (5000, this, 0);
	}

	//Cheanges the quelity of one pre-defined link
	void change_link(node_id_t n1, node_id_t n2, uint16_t etx) {
		wiselib::links_t* link = wiselib::getLink(n1, n2);
		if (link != NULL) {
			debug_->debug("%d: APP: Link %d - %d changed from %d to %d\n",radio_->id(),n1,n2,link->etx, etx);
			link->etx = etx;
		}
	}

	// --------------------------------------------------------------------
	void receive_radio_message(node_id_t from, Radio::size_t len,
		block_data_t *buf) {
			debug_->debug("%d: APP: Message %s reached the root from %d\n",
				radio_->id(), buf, from);
	}

	void receive_serial( Uart::size_t len, Uart::block_data_t *buf )
	{

		debug_->debug("APP received serial message");
		if (len<4) {
			debug_->debug("APP: Unexpected serial message");
			return;
		}
		debug_->debug( "APP: UART: received new ETX: n1 = 5d, n2 = %d, etx = 5d",buf[0],buf[1],(buf[2]<<8)|buf[3] );

		change_link(buf[0],buf[1],(buf[2]<<8)|buf[3]);

		//uart_->write( len, (Uart::block_data_t*)buf );
	}

	//Changes in network topology used during tests
	void first_change(void*) {
		debug_->debug("%d: APP First Change\n",radio_->id());
		wiselib::links_t* link = &wiselib::links[1];
		link->n1=100;
		link->n2=100;
		link->etx=USHRT_MAX;

		timer_->set_timer < CtpTest, &CtpTest::second_change > (40000, this, 0);
	}

	void second_change(void*) {
		debug_->debug("%d: APP: Second Change\n",radio_->id());
		wiselib::links_t* link = &wiselib::links[3];
		link->n1=2;
		link->n2=0;
		link->etx=60;
		//timer_->set_timer < CtpTest, &CtpTest::second_change > (20000, this, 0);
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
