/*
 * Simple Wiselib Example
 */
#include "external_interface/external_interface.h"
#include "algorithms/routing/ctp/ctp_routing_engine.h"
#include "internal_interface/routing_table/routing_table_static_array.h"
#include "algorithms/routing/ctp/ctp_random_number.h"

typedef wiselib::OSMODEL Os;
typedef wiselib::vector_static<Os, Os::Radio::node_id_t, 10> node_vector_t;
typedef wiselib::StaticArrayRoutingTable<Os, Os::Radio, 10> routing_table_t;
//typedef wiselib::CtpRandomNumber<Os, Os::Clock, Os::Debug> random_number_t;
typedef wiselib::CtpRandomNumber<Os> random_number_t;
//typedef wiselib::CtpRoutingEngine<Os, node_vector_t, random_number_t, Os::Radio, Os::Timer, Os::Debug, Os::Clock> re_t;
typedef wiselib::CtpRoutingEngine<Os, node_vector_t, random_number_t> re_t;
class ExampleApplication {
public:
	void init(Os::AppMainParameter& value) {
		radio_ = &wiselib::FacetProvider<Os, Os::Radio>::get_facet(value);
		timer_ = &wiselib::FacetProvider<Os, Os::Timer>::get_facet(value);
		debug_ = &wiselib::FacetProvider<Os, Os::Debug>::get_facet(value);
		clock_ = &wiselib::FacetProvider<Os, Os::Clock>::get_facet(value);


		re_.init();
//		re_.init(*radio_, *timer_, *debug_, *clock_, *random_number_);

		debug_->debug("Hello World from Example Application!\n");

		radio_->reg_recv_callback<ExampleApplication,
				&ExampleApplication::receive_radio_message>(this);
		timer_->set_timer<ExampleApplication, &ExampleApplication::start>(5000,
				this, 0);

	}
	// --------------------------------------------------------------------
	void start(void*) {
		debug_->debug("broadcast message at %d \n", radio_->id());
		Os::Radio::block_data_t message[] = "hello world!\0";
		radio_->send(Os::Radio::BROADCAST_ADDRESS, sizeof(message), message);

		// following can be used for periodic messages to sink
		// timer_->set_timer<ExampleApplication,
		//                  &ExampleApplication::start>( 5000, this, 0 );
	}
	// --------------------------------------------------------------------
	void receive_radio_message(Os::Radio::node_id_t from, Os::Radio::size_t len,
			Os::Radio::block_data_t *buf) {
		debug_->debug("received msg at %u from %u\n", radio_->id(), from);
		debug_->debug("  message is %s\n", buf);
	}
private:
	Os::Radio::self_pointer_t radio_;
	Os::Timer::self_pointer_t timer_;
	Os::Debug::self_pointer_t debug_;
	Os::Clock::self_pointer_t clock_;
	random_number_t::self_pointer_t random_number_;
	re_t re_;
};
// --------------------------------------------------------------------------
wiselib::WiselibApplication<Os, ExampleApplication> example_app;
// --------------------------------------------------------------------------
void application_main(Os::AppMainParameter& value) {
	example_app.init(value);
}
