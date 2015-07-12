#ifndef __SYNC_MSG_H
#define __SYNC_MSG_H

#include "actor_framework.h"
#include "msg_queue.h"
#include "try_move.h"

struct sync_csp_exception {};

struct sync_csp_close_exception : public sync_csp_exception {};

struct sync_msg_close_exception : public sync_csp_close_exception {};

struct csp_channel_close_exception : public sync_csp_close_exception {};

struct csp_try_invoke_exception : public sync_csp_exception {};

struct csp_timed_invoke_exception : public sync_csp_exception {};

struct csp_try_wait_exception : public sync_csp_exception {};

struct csp_timed_wait_exception : public sync_csp_exception {};


//////////////////////////////////////////////////////////////////////////
template <typename TP, typename TM>
struct send_move : public try_move<TM>
{
};

template <typename TP, typename TM>
struct send_move<TP&, TM&&>
{
	static inline TM& move(TM& p0)
	{
		return p0;
	}
};

template <typename TP, typename TM>
struct send_move<TP&&, TM&&>
{
	static inline TM&& move(TM& p0)
	{
		return (TM&&)p0;
	}
};
//////////////////////////////////////////////////////////////////////////
template <typename TP>
struct take_move
{
	static inline TP&& move(TP& p0)
	{
		return (TP&&)p0;
	}
};

template <typename TP>
struct take_move<TP&>
{
	static inline TP& move(TP& p0)
	{
		return p0;
	}
};

template <typename TP>
struct take_move<TP&&>
{
	static inline TP&& move(TP& p0)
	{
		return (TP&&)p0;
	}
};

/*!
@brief 同步发送消息（多读多写，角色可转换），发送方等到消息取出才返回
*/
template <typename T>
class sync_msg
{
	typedef RM_REF(T) ST;

	template <typename T>
	struct result_type
	{
		typedef T type;
	};

	template <typename T>
	struct result_type<T&>
	{
		typedef std::reference_wrapper<T> type;
	};

	template <typename T>
	struct result_type<T&&>
	{
		typedef T type;
	};

	typedef typename result_type<T>::type RT;

	//////////////////////////////////////////////////////////////////////////
	struct send_wait
	{
		bool can_move;
		bool& notified;
		ST& src_msg;
		actor_trig_notifer<bool> ntf;
	};

	struct take_wait
	{
		bool* can_move;
		bool& notified;
		unsigned char* dst;
		actor_trig_notifer<bool> ntf;
		actor_trig_notifer<bool>& takeOk;
	};
public:
	struct close_exception : public sync_msg_close_exception {};
public:
	sync_msg(shared_strand strand)
		:_closed(false), _strand(strand), _sendWait(4), _takeWait(4) {}
public:
	/*!
	@brief 同步发送一条消息，直到对方取出后返回
	*/
	template <typename TM>
	void send(my_actor* host, TM&& msg)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		bool closed = false;
		bool notified = false;
		LAMBDA_THIS_REF5(ref5, host, msg, ath, closed, notified);
		host->send(_strand, [&ref5]
		{
			if (ref5->_closed)
			{
				ref5.closed = true;
			} 
			else
			{
				auto& _takeWait = ref5->_takeWait;
				if (_takeWait.empty())
				{
					send_wait pw = { try_move<TM&&>::can_move, ref5.notified, (ST&)ref5.msg, ref5.host->make_trig_notifer(ref5.ath) };
					ref5->_sendWait.push_front(pw);
				}
				else
				{
					take_wait& wt = _takeWait.back();
					new(wt.dst)RT(send_move<T, TM&&>::move(ref5.msg));
					if (wt.can_move)
					{
						*wt.can_move = try_move<TM&&>::can_move || !(std::is_reference<T>::value);
					}
					wt.notified = true;
					wt.takeOk = ref5.host->make_trig_notifer(ref5.ath);
					wt.ntf(false);
					_takeWait.pop_back();
				}
			}
		});
		if (!closed)
		{
			closed = host->wait_trig(ath);
		}
		if (closed)
		{
			qg.unlock();
			throw close_exception();
		}
	}

	/*!
	@brief 尝试同步发送一条消息，如果对方在等待消息则成功
	@return 成功返回true
	*/
	template <typename TM>
	bool try_send(my_actor* host, TM&& msg)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		bool ok = false;
		bool closed = false;
		LAMBDA_THIS_REF5(ref5, host, msg, ath, ok, closed);
		host->send(_strand, [&ref5]
		{
			if (ref5->_closed)
			{
				ref5.closed = true;
			} 
			else
			{
				auto& _takeWait = ref5->_takeWait;
				if (!_takeWait.empty())
				{
					ref5.ok = true;
					take_wait& wt = _takeWait.back();
					new(wt.dst)RT(send_move<T, TM&&>::move(ref5.msg));
					if (wt.can_move)
					{
						*wt.can_move = try_move<TM&&>::can_move || !(std::is_reference<T>::value);
					}
					wt.takeOk = ref5.host->make_trig_notifer(ref5.ath);
					wt.ntf(false);
					_takeWait.pop_back();
				}
			}
		});
		if (!closed && ok)
		{
			closed = host->wait_trig(ath);
		}
		if (closed)
		{
			qg.unlock();
			throw close_exception();
		}
		return ok;
	}

	/*!
	@brief 尝试同步发送一条消息，对方在一定时间内取出则成功
	@return 成功返回true
	*/
	template <typename TM>
	bool timed_send(int tm, my_actor* host, TM&& msg)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		msg_list<send_wait>::iterator mit;
		bool closed = false;
		bool notified = false;
		LAMBDA_THIS_REF6(ref6, host, msg, ath, mit, closed, notified);
		host->send(_strand, [&ref6]
		{
			if (ref6->_closed)
			{
				ref6.closed = true;
			} 
			else
			{
				auto& _takeWait = ref6->_takeWait;
				if (_takeWait.empty())
				{
					send_wait pw = { try_move<TM&&>::can_move, ref6.notified, (T&)ref6.msg, ref6.host->make_trig_notifer(ref6.ath) };
					ref6->_sendWait.push_front(pw);
					ref6.mit = ref6->_sendWait.begin();
				}
				else
				{
					take_wait& wt = _takeWait.back();
					new(wt.dst)RT(send_move<T, TM&&>::move(ref6.msg));
					if (wt.can_move)
					{
						*wt.can_move = try_move<TM&&>::can_move || !(std::is_reference<T>::value);
					}
					wt.notified = true;
					wt.takeOk = ref6.host->make_trig_notifer(ref6.ath);
					wt.ntf(false);
					_takeWait.pop_back();
				}
			}
		});
		if (!closed)
		{
			if (!host->timed_wait_trig(tm, ath, closed))
			{
				host->send(_strand, [&ref6]
				{
					if (!ref6.notified)
					{
						ref6->_sendWait.erase(ref6.mit);
					}
				});
				if (notified)
				{
					closed = host->wait_trig(ath);
				}
			}
		}
		if (closed)
		{
			qg.unlock();
			throw close_exception();
		}
		return notified;
	}

	/*!
	@brief 取出一条消息，直到有消息过来才返回
	*/
	RT take(my_actor* host)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		actor_trig_notifer<bool> ntf;
		unsigned char msgBuf[sizeof(RT)];
		bool wait = false;
		bool closed = false;
		bool notified = false;
		LAMBDA_THIS_REF7(ref6, host, ath, ntf, msgBuf, wait, closed, notified);
		host->send(_strand, [&ref6]
		{
			if (ref6->_closed)
			{
				ref6.closed = true;
			} 
			else
			{
				auto& _sendWait = ref6->_sendWait;
				if (_sendWait.empty())
				{
					ref6.wait = true;
					take_wait pw = { NULL, ref6.notified, ref6.msgBuf, ref6.host->make_trig_notifer(ref6.ath), ref6.ntf };
					ref6->_takeWait.push_front(pw);
				}
				else
				{
					send_wait& wt = _sendWait.back();
					new(ref6.msgBuf)RT(wt.can_move ? take_move<T>::move(wt.src_msg) : wt.src_msg);
					wt.notified = true;
					ref6.ntf = wt.ntf;
					_sendWait.pop_back();
				}
			}
		});
		if (wait)
		{
			closed = host->wait_trig(ath);
		}
		if (!closed)
		{
			AUTO_CALL(
			{
				typedef RT TP_;
				((TP_*)msgBuf)->~TP_();
				ntf(false);
			});
			return std::move((RM_REF(RT)&)*(RT*)msgBuf);
		}
		qg.unlock();
		throw close_exception();
	}

	/*!
	@brief 尝试取出一条消息，如果有就成功
	@return 成功返回true
	*/
	template <typename TM>
	bool try_take(my_actor* host, TM& out)
	{
		return try_take_ct(host, [&](bool rval, ST& msg)
		{
			out = rval ? std::move(msg) : msg;
		});
	}

	bool try_take(my_actor* host, stack_obj<T>& out)
	{
		return try_take_ct(host, [&](bool rval, ST& msg)
		{
			out.create(rval ? std::move(msg) : msg);
		});
	}

	/*!
	@brief 尝试取出一条消息，在一定时间内取到就成功
	@return 成功返回true
	*/
	template <typename TM>
	bool timed_take(int tm, my_actor* host, TM& out)
	{
		return timed_take_ct(tm, host, [&](bool rval, ST& msg)
		{
			out = rval ? std::move(msg) : msg;
		});
	}

	bool timed_take(int tm, my_actor* host, stack_obj<T>& out)
	{
		return timed_take_ct(tm, host, [&](bool rval, ST& msg)
		{
			out.create(rval ? std::move(msg) : msg);
		});
	}

	/*!
	@brief 关闭消息通道，所有执行者将抛出 close_exception 异常
	*/
	void close(my_actor* host)
	{
		my_actor::quit_guard qg(host);
		host->send(_strand, [this]
		{
			_closed = true;
			while (!_takeWait.empty())
			{
				auto& wt = _takeWait.front();
				wt.notified = true;
				wt.ntf(true);
				_takeWait.pop_front();
			}
			while (!_sendWait.empty())
			{
				auto& st = _sendWait.front();
				st.notified = true;
				st.ntf(true);
				_sendWait.pop_front();
			}
		});
	}

	/*!
	@brief close 后重置
	*/
	void reset()
	{
		assert(_closed);
		_closed = false;
	}
private:
	template <typename CT>
	bool try_take_ct(my_actor* host, const CT& ct)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		bool ok = false;
		bool closed = false;
		LAMBDA_THIS_REF5(ref5, host, ct, ath, ok, closed);
		host->send(_strand, [&ref5]
		{
			if (ref5->_closed)
			{
				ref5.closed = true;
			}
			else
			{
				auto& _sendWait = ref5->_sendWait;
				if (!_sendWait.empty())
				{
					ref5.ok = true;
					send_wait& wt = _sendWait.back();
					ref5.ct(wt.can_move, wt.src_msg);
					wt.notified = true;
					wt.ntf(false);
					_sendWait.pop_back();
				}
			}
		});
		if (closed)
		{
			qg.unlock();
			throw close_exception();
		}
		return ok;
	}

	template <typename CT>
	bool timed_take_ct(int tm, my_actor* host, const CT& ct)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		actor_trig_notifer<bool> ntf;
		msg_list<take_wait>::iterator mit;
		unsigned char msgBuf[sizeof(RT)];
		bool wait = false;
		bool closed = false;
		bool notified = false;
		bool can_move = false;
		LAMBDA_REF5(ref4, wait, closed, notified, can_move, ntf);
		LAMBDA_THIS_REF6(ref6, ref4, host, ct, ath, mit, msgBuf);
		host->send(_strand, [&ref6]
		{
			auto& ref4 = ref6.ref4;
			if (ref6->_closed)
			{
				ref4.closed = true;
			}
			else
			{
				auto& _sendWait = ref6->_sendWait;
				if (_sendWait.empty())
				{
					ref4.wait = true;
					take_wait pw = { &ref4.can_move, ref4.notified, ref6.msgBuf, ref6.host->make_trig_notifer(ref6.ath), ref4.ntf };
					ref6->_takeWait.push_front(pw);
					ref6.mit = ref6->_takeWait.begin();
				}
				else
				{
					send_wait& wt = _sendWait.back();
					ref6.ct(wt.can_move, wt.src_msg);
					wt.notified = true;
					wt.ntf(false);
					_sendWait.pop_back();
				}
			}
		});
		bool ok = true;
		if (wait)
		{
			if (!host->timed_wait_trig(tm, ath, closed))
			{
				host->send(_strand, [&ref6]
				{
					if (!ref6.ref4.notified)
					{
						ref6->_takeWait.erase(ref6.mit);
					}
				});
				ok = notified;
				if (notified)
				{
					closed = host->wait_trig(ath);
				}
			}
			if (notified && !closed)
			{
				ct(can_move, *(RT*)msgBuf);
				((RT*)msgBuf)->~RT();
				ntf(false);
			}
		}
		if (closed)
		{
			qg.unlock();
			throw close_exception();
		}
		return ok;
	}

	sync_msg(const sync_msg&){};
	void operator=(const sync_msg&){};
private:
	bool _closed;
	shared_strand _strand;
	msg_list<take_wait> _takeWait;
	msg_list<send_wait> _sendWait;
};

/*!
@brief csp函数调用，不直接使用
*/
template <typename T, typename R>
class csp_channel
{
	struct send_wait
	{
		bool& notified;
		T& srcMsg;
		unsigned char* res;
		actor_trig_notifer<bool> ntf;
	};

	struct take_wait
	{
		bool& notified;
		T*& srcMsg;
		unsigned char*& res;
		actor_trig_notifer<bool> ntf;
		actor_trig_notifer<bool>& ntfSend;
	};
protected:
	csp_channel(shared_strand strand)
		:_closed(false), _strand(strand), _takeWait(4), _sendWait(4)
	{
		DEBUG_OPERATION(_thrownCloseExp = false);
	}
	~csp_channel() {}
public:
	/*!
	@brief 开始准备执行对方的一个函数，执行完毕后返回结果
	@return 返回结果
	*/
	template <typename TM>
	R send(my_actor* host, TM&& msg)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		unsigned char resBuf[sizeof(R)];
		bool closed = false;
		bool notified = false;
		LAMBDA_THIS_REF6(ref6, host, msg, ath, resBuf, closed, notified);
		host->send(_strand, [&ref6]
		{
			if (ref6->_closed)
			{
				ref6.closed = true;
			}
			else
			{
				auto& _takeWait = ref6->_takeWait;
				if (_takeWait.empty())
				{
					send_wait pw = { ref6.notified, ref6.msg, ref6.resBuf, ref6.host->make_trig_notifer(ref6.ath) };
					ref6->_sendWait.push_front(pw);
				}
				else
				{
					take_wait& wt = _takeWait.back();
					wt.srcMsg = &ref6.msg;
					wt.notified = true;
					wt.ntf(false);
					wt.res = ref6.resBuf;
					wt.ntfSend = ref6.host->make_trig_notifer(ref6.ath);
					_takeWait.pop_back();
				}
			}
		});
		if (!closed)
		{
			closed = host->wait_trig(ath);
		}
		if (closed)
		{
			qg.unlock();
			throw_close_exception();
		}
		AUTO_CALL(
		{
			typedef R TP_;
			((TP_*)resBuf)->~TP_();
		});
		return std::move(*(R*)resBuf);
	}

	/*!
	@brief 尝试执行对方的一个函数，如果对方在等待执行就成功，失败抛出 try_invoke_exception 异常
	@return 成功返回结果
	*/
	template <typename TM>
	R try_send(my_actor* host, TM&& msg)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		unsigned char resBuf[sizeof(R)];
		bool closed = false;
		bool has = false;
		LAMBDA_THIS_REF6(ref6, host, msg, ath, resBuf, closed, has);
		host->send(_strand, [&ref6]
		{
			if (ref6->_closed)
			{
				ref6.closed = true;
			}
			else
			{
				auto& _takeWait = ref6->_takeWait;
				if (!_takeWait.empty())
				{
					ref6.has = true;
					take_wait& wt = _takeWait.back();
					wt.srcMsg = &ref6.msg;
					wt.notified = true;
					wt.ntf(false);
					wt.res = ref6.resBuf;
					wt.ntfSend = ref6.host->make_trig_notifer(ref6.ath);
					_takeWait.pop_back();
				}
			}
		});
		if (!closed)
		{
			if (!has)
			{
				qg.unlock();
				throw_try_send_exception();
			}
			closed = host->wait_trig(ath);
		}
		if (closed)
		{
			qg.unlock();
			throw_close_exception();
		}
		AUTO_CALL(
		{
			typedef R TP_;
			((TP_*)resBuf)->~TP_();
		});
		return std::move(*(R*)resBuf);
	}

	/*!
	@brief 尝试执行对方的一个函数，如果对方在一定时间内提供函数就成功，失败抛出 timed_invoke_exception 异常
	@return 成功返回结果
	*/
	template <typename TM>
	R timed_send(int tm, my_actor* host, TM&& msg)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		unsigned char resBuf[sizeof(R)];
		msg_list<send_wait>::iterator nit;
		bool closed = false;
		bool notified = false;
		bool has = false;
		LAMBDA_THIS_REF3(ref3, closed, notified, has);
		LAMBDA_THIS_REF6(ref6, ref3, host, msg, ath, resBuf, nit);
		host->send(_strand, [&ref6]
		{
			auto& ref3 = ref6.ref3;
			if (ref3->_closed)
			{
				ref3.closed = true;
			}
			else
			{
				auto& _takeWait = ref6->_takeWait;
				if (_takeWait.empty())
				{
					send_wait pw = { ref3.notified, ref6.msg, ref6.resBuf, ref6.host->make_trig_notifer(ref6.ath) };
					ref6->_sendWait.push_front(pw);
					ref6.nit = ref6->_sendWait.begin();
				}
				else
				{
					ref3.has = true;
					take_wait& wt = _takeWait.back();
					wt.srcMsg = &ref6.msg;
					wt.notified = true;
					wt.ntf(false);
					wt.res = ref6.resBuf;
					wt.ntfSend = ref6.host->make_trig_notifer(ref6.ath);
					_takeWait.pop_back();
				}
			}
		});
		if (!closed)
		{
			if (!has)
			{
				if (!host->timed_wait_trig(tm, ath, closed))
				{
					host->send(_strand, [&ref6]
					{
						if (ref6.ref3.notified)
						{
							ref6.ref3.has = true;
						}
						else
						{
							ref6->_sendWait.erase(ref6.nit);
						}
					});
					if (!has)
					{
						qg.unlock();
						throw_timed_send_exception();
					}
				}
			}
			if (has)
			{
				closed = host->wait_trig(ath);
			}
		}
		if (closed)
		{
			qg.unlock();
			throw_close_exception();
		}
		AUTO_CALL(
		{
			typedef R TP_;
			((TP_*)resBuf)->~TP_();
		});
		return std::move(*(R*)resBuf);
	}

	/*!
	@brief 等待对方执行一个函数体，函数体内返回对方需要的执行结果
	*/
	template <typename H>
	void take(my_actor* host, const H& h)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		actor_trig_notifer<bool> ntfSend;
		T* srcMsg = NULL;
		unsigned char* res = NULL;
		bool wait = false;
		bool closed = false;
		bool notified = false;
		LAMBDA_REF3(ref3, wait, closed, notified);
		LAMBDA_THIS_REF6(ref6, ref3, host, ath, ntfSend, srcMsg, res);
		host->send(_strand, [&ref6]
		{
			auto& ref3 = ref6.ref3;
			if (ref6->_closed)
			{
				ref3.closed = true;
			}
			else
			{
				auto& _sendWait = ref6->_sendWait;
				if (_sendWait.empty())
				{
					ref3.wait = true;
					take_wait pw = { ref3.notified, ref6.srcMsg, ref6.res, ref6.host->make_trig_notifer(ref6.ath), ref6.ntfSend };
					ref6->_takeWait.push_front(pw);
				}
				else
				{
					send_wait& wt = _sendWait.back();
					ref6.srcMsg = &wt.srcMsg;
					wt.notified = true;
					ref6.ntfSend = wt.ntf;
					ref6.res = wt.res;
					_sendWait.pop_back();
				}
			}
		});
		if (wait)
		{
			closed = host->wait_trig(ath);
		}
		if (!closed)
		{
			bool ok = false;
			AUTO_CALL({ ntfSend(!ok); });
			BEGIN_TRY_
			{
				new(res)R(h(*srcMsg));
			}
			CATCH_FOR(sync_csp_close_exception)
			{
				assert(_thrownCloseExp);
				DEBUG_OPERATION(_thrownCloseExp = false);
				qg.unlock();
				throw_close_exception();
			}
			END_TRY_;
			ok = true;
			return;
		}
		qg.unlock();
		throw_close_exception();
	}

	/*!
	@brief 尝试执行一个函数体，如果对方在准备执行则成功，否则抛出 try_wait_exception 异常
	*/
	template <typename H>
	void try_take(my_actor* host, const H& h)
	{
		my_actor::quit_guard qg(host);
		actor_trig_notifer<bool> ntfSend;
		T* srcMsg = NULL;
		unsigned char* res = NULL;
		bool closed = false;
		bool has = false;
		LAMBDA_REF2(ref2, closed, has);
		LAMBDA_THIS_REF5(ref5, ref2, host, ntfSend, srcMsg, res);
		host->send(_strand, [&ref5]
		{
			auto& ref2 = ref5.ref2;
			if (ref5->_closed)
			{
				ref2.closed = true;
			}
			else
			{
				auto& _sendWait = ref5->_sendWait;
				if (!_sendWait.empty())
				{
					ref2.has = true;
					send_wait& wt = _sendWait.back();
					ref5.srcMsg = &wt.srcMsg;
					wt.notified = true;
					ref5.ntfSend = wt.ntf;
					ref5.res = wt.res;
					_sendWait.pop_back();
				}
			}
		});
		if (closed)
		{
			qg.unlock();
			throw_close_exception();
		}
		if (!has)
		{
			qg.unlock();
			throw_try_take_exception();
		}
		bool ok = false;
		AUTO_CALL({ ntfSend(!ok); });
		BEGIN_TRY_
		{
			new(res)R(h(*srcMsg));
		}
		CATCH_FOR(sync_csp_close_exception)
		{
			assert(_thrownCloseExp);
			DEBUG_OPERATION(_thrownCloseExp = false);
			qg.unlock();
			throw_close_exception();
		}
		END_TRY_;
		ok = true;
	}

	/*!
	@brief 尝试执行一个函数体，如果对方在一定时间内执行则成功，否则抛出 timed_wait_exception 异常
	*/
	template <typename H>
	void timed_take(int tm, my_actor* host, const H& h)
	{
		my_actor::quit_guard qg(host);
		actor_trig_handle<bool> ath;
		actor_trig_notifer<bool> ntfSend;
		msg_list<take_wait>::iterator wit;
		T* srcMsg = NULL;
		unsigned char* res = NULL;
		bool wait = false;
		bool closed = false;
		bool notified = false;
		LAMBDA_REF4(ref4, wait, closed, notified, wit);
		LAMBDA_THIS_REF6(ref6, ref4, host, ath, ntfSend, srcMsg, res);
		host->send(_strand, [&ref6]
		{
			auto& ref4 = ref6.ref4;
			if (ref6->_closed)
			{
				ref4.closed = true;
			}
			else
			{
				auto& _sendWait = ref6->_sendWait;
				if (_sendWait.empty())
				{
					ref4.wait = true;
					take_wait pw = { ref4.notified, ref6.srcMsg, ref6.res, ref6.host->make_trig_notifer(ref6.ath), ref6.ntfSend };
					ref6->_takeWait.push_front(pw);
					ref4.wit = ref6->_takeWait.begin();
				}
				else
				{
					send_wait& wt = _sendWait.back();
					ref6.srcMsg = &wt.srcMsg;
					wt.notified = true;
					ref6.ntfSend = wt.ntf;
					ref6.res = wt.res;
					_sendWait.pop_back();
				}
			}
		});
		if (!closed)
		{
			if (wait)
			{
				if (!host->timed_wait_trig(tm, ath, closed))
				{
					host->send(_strand, [&ref6]
					{
						auto& ref4 = ref6.ref4;
						ref4.wait = ref4.notified;
						if (!ref4.notified)
						{
							ref6->_takeWait.erase(ref4.wit);
						}
					});
					if (wait)
					{
						closed = host->wait_trig(ath);
					}
					if (!wait && !closed)
					{
						qg.unlock();
						throw_timed_take_exception();
					}
				}
			}
		}
		if (closed)
		{
			qg.unlock();
			throw_close_exception();
		}
		bool ok = false;
		AUTO_CALL({ ntfSend(!ok); });
		BEGIN_TRY_
		{
			new(res)R(h(*srcMsg));
		}
		CATCH_FOR(sync_csp_close_exception)
		{
			assert(_thrownCloseExp);
			DEBUG_OPERATION(_thrownCloseExp = false);
			qg.unlock();
			throw_close_exception();
		}
		END_TRY_;
		ok = true;
	}

	/*!
	@brief 关闭执行通道，所有执行抛出 close_exception 异常
	*/
	void close(my_actor* host)
	{
		my_actor::quit_guard qg(host);
		host->send(_strand, [this]
		{
			_closed = true;
			while (!_takeWait.empty())
			{
				auto& wt = _takeWait.front();
				wt.notified = true;
				wt.ntf(true);
// 				if (!wt.ntfSend.empty())
// 				{
// 					wt.ntfSend(true);
// 				}
				_takeWait.pop_front();
			}
			while (!_sendWait.empty())
			{
				auto& st = _sendWait.front();
				st.notified = true;
				st.ntf(true);
				_sendWait.pop_front();
			}
		});
	}

	/*!
	@brief close 后重置
	*/
	void reset()
	{
		assert(_closed);
		_closed = false;
		DEBUG_OPERATION(_thrownCloseExp = false);
	}
private:
	csp_channel(const csp_channel&){};
	void operator=(const csp_channel&){};

	virtual void throw_close_exception() = 0;
	virtual void throw_try_send_exception() = 0;
	virtual void throw_timed_send_exception() = 0;
	virtual void throw_try_take_exception() = 0;
	virtual void throw_timed_take_exception() = 0;
private:
	bool _closed;
	shared_strand _strand;
	msg_list<take_wait> _takeWait;
	msg_list<send_wait> _sendWait;
protected:
	DEBUG_OPERATION(bool _thrownCloseExp);
};

//////////////////////////////////////////////////////////////////////////

template <typename T0, typename T1, typename T2, typename T3, typename R = void>
class csp_invoke_base4 : public csp_channel<ref_ex<T0, T1, T2, T3>, R>
{
	typedef ref_ex<T0, T1, T2, T3> msg_type;
	typedef csp_channel<msg_type, R> base_csp_channel;
protected:
	csp_invoke_base4(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base4() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1, msg._p2, msg._p3);
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1, msg._p2, msg._p3);
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1, msg._p2, msg._p3);
		});
	}

	template <typename TM0, typename TM1, typename TM2, typename TM3>
	R invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2, TM3&& p3)
	{
		return base_csp_channel::send(host, bind_ref(p0, p1, p2, p3));
	}

	template <typename TM0, typename TM1, typename TM2, typename TM3>
	R try_invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2, TM3&& p3)
	{
		return base_csp_channel::try_send(host, bind_ref(p0, p1, p2, p3));
	}

	template <typename TM0, typename TM1, typename TM2, typename TM3>
	R timed_invoke(int tm, my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2, TM3&& p3)
	{
		return base_csp_channel::timed_send(tm, host, bind_ref(p0, p1, p2, p3));
	}
};

template <typename T0, typename T1, typename T2, typename R = void>
class csp_invoke_base3 : public csp_channel<ref_ex<T0, T1, T2>, R>
{
	typedef ref_ex<T0, T1, T2> msg_type;
	typedef csp_channel<msg_type, R> base_csp_channel;
protected:
	csp_invoke_base3(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base3() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1, msg._p2);
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1, msg._p2);
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1, msg._p2);
		});
	}

	template <typename TM0, typename TM1, typename TM2>
	R invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2)
	{
		return base_csp_channel::send(host, bind_ref(p0, p1, p2));
	}

	template <typename TM0, typename TM1, typename TM2>
	R try_invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2)
	{
		return base_csp_channel::try_send(host, bind_ref(p0, p1, p2));
	}

	template <typename TM0, typename TM1, typename TM2>
	R timed_invoke(int tm, my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2)
	{
		return base_csp_channel::timed_send(tm, host, bind_ref(p0, p1, p2));
	}
};

template <typename T0, typename T1, typename R = void>
class csp_invoke_base2 : public csp_channel<ref_ex<T0, T1>, R>
{
	typedef ref_ex<T0, T1> msg_type;
	typedef csp_channel<msg_type, R> base_csp_channel;
protected:
	csp_invoke_base2(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base2() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1);
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1);
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->R
		{
			return h(msg._p0, msg._p1);
		});
	}

	template <typename TM0, typename TM1>
	R invoke(my_actor* host, TM0&& p0, TM1&& p1)
	{
		return base_csp_channel::send(host, bind_ref(p0, p1));
	}

	template <typename TM0, typename TM1>
	R try_invoke(my_actor* host, TM0&& p0, TM1&& p1)
	{
		return base_csp_channel::try_send(host, bind_ref(p0, p1));
	}

	template <typename TM0, typename TM1>
	R timed_invoke(int tm, my_actor* host, TM0&& p0, TM1&& p1)
	{
		return base_csp_channel::timed_send(tm, host, bind_ref(p0, p1));
	}
};

template <typename T0, typename R = void>
class csp_invoke_base1 : public csp_channel<ref_ex<T0>, R>
{
	typedef ref_ex<T0> msg_type;
	typedef csp_channel<msg_type, R> base_csp_channel;
protected:
	csp_invoke_base1(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base1() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0);
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->R
		{
			return h(msg._p0);
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->R
		{
			return h(msg._p0);
		});
	}

	template <typename TM0>
	R invoke(my_actor* host, TM0&& p0)
	{
		return base_csp_channel::send(host, bind_ref(p0));
	}

	template <typename TM0>
	R try_invoke(my_actor* host, TM0&& p0)
	{
		return base_csp_channel::try_send(host, bind_ref(p0));
	}

	template <typename TM0>
	R timed_invoke(int tm, my_actor* host, TM0&& p0)
	{
		return base_csp_channel::timed_send(tm, host, bind_ref(p0));
	}
};

template <typename R = void>
class csp_invoke_base0 : public csp_channel<ref_ex<>, R>
{
	typedef ref_ex<> msg_type;
	typedef csp_channel<msg_type, R> base_csp_channel;
protected:
	csp_invoke_base0(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base0() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->R
		{
			return h();
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->R
		{
			return h();
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->R
		{
			return h();
		});
	}

	R invoke(my_actor* host)
	{
		return base_csp_channel::send(host, msg_type());
	}

	R try_invoke(my_actor* host)
	{
		return base_csp_channel::try_send(host, msg_type());
	}

	R timed_invoke(int tm, my_actor* host)
	{
		return base_csp_channel::timed_send(tm, host, msg_type());
	}
};

struct void_return
{
};

template <typename T0, typename T1, typename T2, typename T3>
class csp_invoke_base4<T0, T1, T2, T3, void> : public csp_channel<ref_ex<T0, T1, T2, T3>, void_return>
{
	typedef ref_ex<T0, T1, T2, T3> msg_type;
	typedef csp_channel<msg_type, void_return> base_csp_channel;
protected:
	csp_invoke_base4(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base4() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1, msg._p2, msg._p3);
			return void_return();
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1, msg._p2, msg._p3);
			return void_return();
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1, msg._p2, msg._p3);
			return void_return();
		});
	}

	template <typename TM0, typename TM1, typename TM2, typename TM3>
	void invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2, TM3&& p3)
	{
		base_csp_channel::send(host, bind_ref(p0, p1, p2, p3));
	}

	template <typename TM0, typename TM1, typename TM2, typename TM3>
	void try_invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2, TM3&& p3)
	{
		base_csp_channel::try_send(host, bind_ref(p0, p1, p2, p3));
	}

	template <typename TM0, typename TM1, typename TM2, typename TM3>
	void timed_invoke(int tm, my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2, TM3&& p3)
	{
		base_csp_channel::timed_send(tm, host, bind_ref(p0, p1, p2, p3));
	}
};

template <typename T0, typename T1, typename T2>
class csp_invoke_base3<T0, T1, T2, void> : public csp_channel<ref_ex<T0, T1, T2>, void_return>
{
	typedef ref_ex<T0, T1, T2> msg_type;
	typedef csp_channel<msg_type, void_return> base_csp_channel;
protected:
	csp_invoke_base3(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base3() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1, msg._p2);
			return void_return();
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1, msg._p2);
			return void_return();
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1, msg._p2);
			return void_return();
		});
	}

	template <typename TM0, typename TM1, typename TM2>
	void invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2)
	{
		base_csp_channel::send(host, bind_ref(p0, p1, p2));
	}

	template <typename TM0, typename TM1, typename TM2>
	void try_invoke(my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2)
	{
		base_csp_channel::try_send(host, bind_ref(p0, p1, p2));
	}

	template <typename TM0, typename TM1, typename TM2>
	void timed_invoke(int tm, my_actor* host, TM0&& p0, TM1&& p1, TM2&& p2)
	{
		base_csp_channel::timed_send(tm, host, bind_ref(p0, p1, p2));
	}
};

template <typename T0, typename T1>
class csp_invoke_base2<T0, T1, void> : public csp_channel<ref_ex<T0, T1>, void_return>
{
	typedef ref_ex<T0, T1> msg_type;
	typedef csp_channel<msg_type, void_return> base_csp_channel;
protected:
	csp_invoke_base2(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base2() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1);
			return void_return();
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1);
			return void_return();
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->void_return
		{
			h(msg._p0, msg._p1);
			return void_return();
		});
	}

	template <typename TM0, typename TM1>
	void invoke(my_actor* host, TM0&& p0, TM1&& p1)
	{
		base_csp_channel::send(host, bind_ref(p0, p1));
	}

	template <typename TM0, typename TM1>
	void try_invoke(my_actor* host, TM0&& p0, TM1&& p1)
	{
		base_csp_channel::try_send(host, bind_ref(p0, p1));
	}

	template <typename TM0, typename TM1>
	void timed_invoke(int tm, my_actor* host, TM0&& p0, TM1&& p1)
	{
		base_csp_channel::timed_send(tm, host, bind_ref(p0, p1));
	}
};

template <typename T0>
class csp_invoke_base1<T0, void> : public csp_channel<ref_ex<T0>, void_return>
{
	typedef ref_ex<T0> msg_type;
	typedef csp_channel<msg_type, void_return> base_csp_channel;
protected:
	csp_invoke_base1(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base1() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0);
			return void_return();
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->void_return
		{
			h(msg._p0);
			return void_return();
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->void_return
		{
			h(msg._p0);
			return void_return();
		});
	}

	template <typename TM0>
	void invoke(my_actor* host, TM0&& p0)
	{
		base_csp_channel::send(host, bind_ref(p0));
	}

	template <typename TM0>
	void try_invoke(my_actor* host, TM0&& p0)
	{
		base_csp_channel::try_send(host, bind_ref(p0));
	}

	template <typename TM0>
	void timed_invoke(int tm, my_actor* host, TM0&& p0)
	{
		base_csp_channel::timed_send(tm, host, bind_ref(p0));
	}
};

template <>
class csp_invoke_base0<void> : public csp_channel<ref_ex<>, void_return>
{
	typedef ref_ex<> msg_type;
	typedef csp_channel<msg_type, void_return> base_csp_channel;
protected:
	csp_invoke_base0(shared_strand strand)
		:base_csp_channel(strand) {}
	~csp_invoke_base0() {}
public:
	template <typename H>
	void wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::take(host, [&](msg_type& msg)->void_return
		{
			h();
			return void_return();
		});
	}

	template <typename H>
	void try_wait_invoke(my_actor* host, const H& h)
	{
		base_csp_channel::try_take(host, [&](msg_type& msg)->void_return
		{
			h();
			return void_return();
		});
	}

	template <typename H>
	void timed_wait_invoke(int tm, my_actor* host, const H& h)
	{
		base_csp_channel::timed_take(tm, host, [&](msg_type& msg)->void_return
		{
			h();
			return void_return();
		});
	}

	void invoke(my_actor* host)
	{
		base_csp_channel::send(host, msg_type());
	}

	void try_invoke(my_actor* host)
	{
		base_csp_channel::try_send(host, msg_type());
	}

	void timed_invoke(int tm, my_actor* host)
	{
		base_csp_channel::timed_send(tm, host, msg_type());
	}
};

template <typename R, typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class csp_invoke;

/*!
@brief CSP模型消息（多读多写，角色可转换，可递归），发送方等消息取出并处理完成后才返回
*/
template <typename R, typename T0, typename T1, typename T2, typename T3>
class csp_invoke
	<R(T0, T1, T2, T3)> : public csp_invoke_base4<T0, T1, T2, T3, R>
{
public:
	struct close_exception : public csp_channel_close_exception {};
	struct try_invoke_exception : public csp_try_invoke_exception {};
	struct timed_invoke_exception : public csp_timed_invoke_exception {};
	struct try_wait_exception : public csp_try_wait_exception {};
	struct timed_wait_exception : public csp_timed_wait_exception {};
public:
	csp_invoke(shared_strand strand)
		:csp_invoke_base4(strand) {}
private:
	void throw_close_exception()
	{
		assert(!_thrownCloseExp);
		DEBUG_OPERATION(_thrownCloseExp = true);
		throw close_exception();
	}

	void throw_try_send_exception()
	{
		throw try_invoke_exception();
	}

	void throw_timed_send_exception()
	{
		throw timed_invoke_exception();
	}

	void throw_try_take_exception()
	{
		throw try_wait_exception();
	}

	void throw_timed_take_exception()
	{
		throw timed_wait_exception();
	}
};

template <typename R, typename T0, typename T1, typename T2>
class csp_invoke
	<R(T0, T1, T2)> : public csp_invoke_base3<T0, T1, T2, R>
{
public:
	struct close_exception : public csp_channel_close_exception {};
	struct try_invoke_exception : public csp_try_invoke_exception {};
	struct timed_invoke_exception : public csp_timed_invoke_exception {};
	struct try_wait_exception : public csp_try_wait_exception {};
	struct timed_wait_exception : public csp_timed_wait_exception {};
public:
	csp_invoke(shared_strand strand)
		:csp_invoke_base3(strand) {}
private:
	void throw_close_exception()
	{
		assert(!_thrownCloseExp);
		DEBUG_OPERATION(_thrownCloseExp = true);
		throw close_exception();
	}

	void throw_try_send_exception()
	{
		throw try_invoke_exception();
	}

	void throw_timed_send_exception()
	{
		throw timed_invoke_exception();
	}

	void throw_try_take_exception()
	{
		throw try_wait_exception();
	}

	void throw_timed_take_exception()
	{
		throw timed_wait_exception();
	}
};

template <typename R, typename T0, typename T1>
class csp_invoke
	<R(T0, T1)> : public csp_invoke_base2<T0, T1, R>
{
public:
	struct close_exception : public csp_channel_close_exception {};
	struct try_invoke_exception : public csp_try_invoke_exception {};
	struct timed_invoke_exception : public csp_timed_invoke_exception {};
	struct try_wait_exception : public csp_try_wait_exception {};
	struct timed_wait_exception : public csp_timed_wait_exception {};
public:
	csp_invoke(shared_strand strand)
		:csp_invoke_base2(strand) {}
private:
	void throw_close_exception()
	{
		assert(!_thrownCloseExp);
		DEBUG_OPERATION(_thrownCloseExp = true);
		throw close_exception();
	}

	void throw_try_send_exception()
	{
		throw try_invoke_exception();
	}

	void throw_timed_send_exception()
	{
		throw timed_invoke_exception();
	}

	void throw_try_take_exception()
	{
		throw try_wait_exception();
	}

	void throw_timed_take_exception()
	{
		throw timed_wait_exception();
	}
};

template <typename R, typename T0>
class csp_invoke
	<R(T0)> : public csp_invoke_base1<T0, R>
{
public:
	struct close_exception : public csp_channel_close_exception {};
	struct try_invoke_exception : public csp_try_invoke_exception {};
	struct timed_invoke_exception : public csp_timed_invoke_exception {};
	struct try_wait_exception : public csp_try_wait_exception {};
	struct timed_wait_exception : public csp_timed_wait_exception {};
public:
	csp_invoke(shared_strand strand)
		:csp_invoke_base1(strand) {}
private:
	void throw_close_exception()
	{
		assert(!_thrownCloseExp);
		DEBUG_OPERATION(_thrownCloseExp = true);
		throw close_exception();
	}

	void throw_try_send_exception()
	{
		throw try_invoke_exception();
	}

	void throw_timed_send_exception()
	{
		throw timed_invoke_exception();
	}

	void throw_try_take_exception()
	{
		throw try_wait_exception();
	}

	void throw_timed_take_exception()
	{
		throw timed_wait_exception();
	}
};

template <typename R>
class csp_invoke
	<R()> : public csp_invoke_base0<R>
{
public:
	struct close_exception : public csp_channel_close_exception {};
	struct try_invoke_exception : public csp_try_invoke_exception {};
	struct timed_invoke_exception : public csp_timed_invoke_exception {};
	struct try_wait_exception : public csp_try_wait_exception {};
	struct timed_wait_exception : public csp_timed_wait_exception {};
public:
	csp_invoke(shared_strand strand)
		:csp_invoke_base0(strand) {}
private:
	void throw_close_exception()
	{
		assert(!_thrownCloseExp);
		DEBUG_OPERATION(_thrownCloseExp = true);
		throw close_exception();
	}

	void throw_try_send_exception()
	{
		throw try_invoke_exception();
	}

	void throw_timed_send_exception()
	{
		throw timed_invoke_exception();
	}

	void throw_try_take_exception()
	{
		throw try_wait_exception();
	}

	void throw_timed_take_exception()
	{
		throw timed_wait_exception();
	}
};

#endif