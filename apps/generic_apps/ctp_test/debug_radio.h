#ifndef WISELIB_FLOOR_GRID_RADIO_MODEL_H
#define WISELIB_FLOOR_GRID_RADIO_MODEL_H

#include "util/base_classes/radio_base.h"
#include "algorithms/routing/ctp/ctp_debugging.h"
#include "algorithms/routing/ctp/ctp_types.h"

namespace wiselib {
template<typename OsModel_P, typename Radio_P = typename OsModel_P::Radio>
class DebugRadio: public RadioBase<OsModel_P, typename Radio_P::node_id_t,
		typename Radio_P::size_t, typename Radio_P::block_data_t, 10> {
public:
	typedef OsModel_P OsModel;
	typedef Radio_P Radio;
	typedef RadioBase<OsModel_P, typename Radio_P::node_id_t,
			typename Radio_P::size_t, typename Radio_P::block_data_t, 10> radio_base_t;
	typedef DebugRadio<OsModel, Radio> self_type;
	typedef self_type* self_pointer_t;

	typedef typename Radio::node_id_t node_id_t;
	typedef typename Radio::size_t size_t;
	typedef typename Radio::block_data_t block_data_t;
	typedef typename Radio::message_id_t message_id_t;

	enum SpecialNodeIds {
		BROADCAST_ADDRESS = Radio::BROADCAST_ADDRESS, ///< All nodes in communication range
		NULL_NODE_ID = Radio::NULL_NODE_ID ///< Unknown/No node id
	};
	// --------------------------------------------------------------------
	enum Restrictions {
		MAX_MESSAGE_LENGTH = Radio::MAX_MESSAGE_LENGTH
	};

	static uint16_t links[LINKS_NR][3];

	DebugRadio() {
	}

	void init(Radio& radio) {
		radio_ = &radio;

		radio_->template reg_recv_callback<self_type, &self_type::receive>(
				this);
	}

	int send(node_id_t id, size_t len, block_data_t *data) {
		return radio_->send(id, len, data);
	}

	int enable_radio() {
		return radio_->enable_radio();
	}

	int disable_radio() {
		return radio_->disable_radio();
	}

	node_id_t id() {
		return radio_->id();
	}

	bool change_link(node_id_t n1, node_id_t n2, node_id_t new_n1,node_id_t new_n2,ctp_etx_t new_etx) {
		int i = find_link(nodes[n1],nodes[n2]);

		if (i >= 0) {
			links[i][0] = new_n1;
			links[i][1] = new_n2;
			links[i][2] = new_etx;
			return true;
		} else {
			return false;
		}

	}

	ctp_etx_t get_link(node_id_t n1, node_id_t n2) {

		int i = find_link(n1, n2);

		if (i >= 0) {
			return links[i][2];
		} else {

			return MAX_LINK_VALUE;
		}
	}

private:

	void receive(node_id_t from, size_t len, block_data_t* data) {
		int self = radio_->id();

		bool is_neighbor = true;

#ifdef CTP_DEBUGGING
		is_neighbor = (find_link(from, self) >= 0);
#endif

		// Only notify if sender is a grid neighbor:
		if (is_neighbor) {
			self_type::notify_receivers(from, len, data);
		}

	}

	int find_link(node_id_t n1, node_id_t n2) {

		for (int i = 0; i < LINKS_NR; i++) {

			if ((links[i][0] >= NODES_NR) || (links[i][1] >= NODES_NR)) {
				continue;
			}

			if (((n1 == nodes[links[i][0]])
					&& (n2 == nodes[links[i][1]]))
					|| ((n1 == nodes[links[i][1]])
							&& (n2 == nodes[links[i][0]]))) {

				return i;
			}
		}

		return -1;
	}

	Radio* radio_;
};

template<typename OsModel_P, typename Radio_P> uint16_t DebugRadio<OsModel_P,
		Radio_P>::links[][3] = LINKS;
}

#endif
