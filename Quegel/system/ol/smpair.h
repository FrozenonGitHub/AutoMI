#ifndef SMPAIR_H
#define SMPAIR_H

#include <vector>
using namespace std;

template <class ValueT, class MessageT>
class SMPair {
	public:
		typedef vector<MessageT> MessageContainer;

		ValueT _value;
		MessageContainer _msg_buf;//incoming msgs
		bool active;

		SMPair(ValueT value, bool is_active)
		{
			_value=value;
			active=is_active;
		}

		inline ValueT& value()
		{
			return _value;
		}

		inline const ValueT& value() const
		{
			return _value;
		}

		inline MessageContainer& mbuf()
		{
			return _msg_buf;
		}

		inline const MessageContainer& mbuf() const
		{
			return _msg_buf;
		}
};

#endif
