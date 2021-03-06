#ifndef __MY_ACTOR_H
#define __MY_ACTOR_H

#include <list>
#include <functional>
#include "io_engine.h"
#include "run_strand.h"
#include "msg_queue.h"
#include "actor_mutex.h"
#include "scattered.h"
#include "stack_object.h"
#include "check_actor_stack.h"
#include "actor_timer.h"
#include "async_timer.h"
#include "context_pool.h"
#include "tuple_option.h"
#include "trace.h"
#include "lambda_ref.h"
#include "trace_stack.h"
#include "generator.h"

class my_actor;
typedef std::shared_ptr<my_actor> actor_handle;//Actor句柄

typedef ContextPool_::coro_push_interface actor_push_type;
typedef ContextPool_::coro_pull_interface actor_pull_type;

//此函数会上下文切换
#define __yield_interrupt

#if (_DEBUG || DEBUG)

// 用于检测在Actor内调用的函数是否触发了强制退出
#define BEGIN_CHECK_FORCE_QUIT try {
#define END_CHECK_FORCE_QUIT } catch (my_actor::force_quit_exception&) {assert(false);}

//用于锁定actor不强制退出
#define LOCK_QUIT(__self__) __self__->lock_quit(); try {
#define UNLOCK_QUIT(__self__) } catch (...) { assert (false); } __self__->unlock_quit();

#else

#define BEGIN_CHECK_FORCE_QUIT
#define END_CHECK_FORCE_QUIT

#define LOCK_QUIT(__self__) __self__->lock_quit();
#define UNLOCK_QUIT(__self__) __self__->unlock_quit();

#endif

//包装msg_handle, trig_handle关闭事件
#define wrap_close_msg_handle(__handle__) [&__handle__]{__handle__.close(); }

//初始化my_actor框架
#define init_my_actor(...)\
	my_actor::install(__VA_ARGS__); \
	BREAK_OF_SCOPE_EXEC(my_actor::uninstall());

struct shared_initer;
//////////////////////////////////////////////////////////////////////////

template <typename... ARGS>
class msg_pump_handle;
class CheckLost_;
class CheckPumpLost_;
class msg_handle_base;
class MsgPoolBase_;

struct ActorFunc_
{
	static void lock_quit(my_actor* host);
	static void unlock_quit(my_actor* host);
	static actor_handle shared_from_this(my_actor* host);
	static const actor_handle& parent_actor(my_actor* host);
	static const shared_strand& self_strand(my_actor* host);
	static void pull_yield(my_actor* host);
	static void push_yield(my_actor* host);
	static void pull_yield_after_quited(my_actor* host);
	static void push_yield_after_quited(my_actor* host);
	static bool is_quited(my_actor* host);
	static reusable_mem& reu_mem(my_actor* host);
	template <typename R, typename H>
	static R send(my_actor* host, const shared_strand& exeStrand, H&& h);
	template <typename R, typename H>
	static R async_send(my_actor* host, const shared_strand& exeStrand, H&& h);
	template <typename... Args>
	static msg_pump_handle<Args...> connect_msg_pump(const int id, my_actor* const host, bool checkLost);
	template <typename H>
	static void delay_trig(my_actor* host, int ms, H&& h);
	static void cancel_timer(my_actor* host);
	template <typename DST, typename SRC>
	static void _trig_handler(my_actor* host, bool* sign, DST& dstRec, SRC&& args);
	template <typename DST, typename SRC>
	static void _trig_handler2(my_actor* host, shared_bool& closed, bool* sign, DST& dstRec, SRC&& args);
#ifdef ENABLE_CHECK_LOST
	static std::shared_ptr<CheckLost_> new_check_lost(const shared_strand& strand, msg_handle_base* msgHandle);
	static std::shared_ptr<CheckPumpLost_> new_check_pump_lost(const actor_handle& hostActor, MsgPoolBase_* pool);
	static std::shared_ptr<CheckPumpLost_> new_check_pump_lost(actor_handle&& hostActor, MsgPoolBase_* pool);
#endif
};
//////////////////////////////////////////////////////////////////////////

/*!
@brief 通知句柄丢失
*/
struct ntf_lost_exception {};

/*!
@brief 消息泵被断开
*/
struct pump_disconnected_exception { };

#ifdef ENABLE_CHECK_LOST
class CheckLost_
{
	friend ActorFunc_;
	FRIEND_SHARED_PTR(CheckLost_);
private:
	CheckLost_(const shared_strand& strand, msg_handle_base* msgHandle);
	~CheckLost_();
private:
	shared_strand _strand;
	shared_bool _closed;
	msg_handle_base* _handle;
};

class CheckPumpLost_
{
	friend ActorFunc_;
	FRIEND_SHARED_PTR(CheckPumpLost_);
private:
	CheckPumpLost_(const actor_handle& hostActor, MsgPoolBase_* pool);
	CheckPumpLost_(actor_handle&& hostActor, MsgPoolBase_* pool);
	~CheckPumpLost_();
private:
	actor_handle _hostActor;
	MsgPoolBase_* _pool;
};
#endif

//////////////////////////////////////////////////////////////////////////

template <typename... TYPES>
struct DstReceiverRef_ {};

template <typename... ArgsPipe>
struct DstReceiverBase_
{
	virtual void move_from(std::tuple<ArgsPipe...>&& s) = 0;
	virtual void clear() = 0;
	virtual bool has() = 0;
};

template <typename... ARGS>
struct DstReceiverBuff_ : public DstReceiverBase_<TYPE_PIPE(ARGS)...>
{
	void move_from(std::tuple<TYPE_PIPE(ARGS)...>&& s)
	{
		_dstBuff.create(std::move(s));
	}

	void clear()
	{
		_dstBuff.destroy();
	}

	bool has()
	{
		return _dstBuff.has();
	}

	stack_obj<std::tuple<TYPE_PIPE(ARGS)...>> _dstBuff;
};

template <typename... ARGS, typename... OUTS>
struct DstReceiverRef_<types_pck<ARGS...>, types_pck<OUTS...>> : public DstReceiverBase_<TYPE_PIPE(ARGS)...>
{
	template <typename... Outs>
	DstReceiverRef_(Outs&... outs)
		:_dstRef(outs...) {}

	void move_from(std::tuple<TYPE_PIPE(ARGS)...>&& s)
	{
		_has = true;
		_dstRef = std::move(s);
	}

	void clear()
	{
		_has = false;
	}

	bool has()
	{
		return _has;
	}

	std::tuple<OUTS&...> _dstRef;
	bool _has = false;
};

//////////////////////////////////////////////////////////////////////////

template <typename... ARGS>
class select_block_msg;

template <typename... ARGS>
class select_block_trig;

template <typename... ARGS>
class select_block_pump;

template <typename... ARGS>
class msg_handle;

template <typename... ARGS>
class trig_handle;

template <typename... ARGS>
class select_block_pump_check_state;

template <typename... ARGS>
class MsgNotiferBase_;

struct select_msg_block_force_quit {};

enum pump_check_state
{
	pump_connected = 0,
	pump_disconnted,
	pump_ntf_losted
};

#ifdef ENABLE_CHECK_LOST

template <typename... ARGS>
class select_block_msg_check_lost;

template <typename... ARGS>
class select_block_trig_check_lost;

template <typename... ARGS>
class select_block_pump_check_lost;

#endif

class msg_handle_base
{
	friend CheckLost_;
protected:
	msg_handle_base();
	virtual ~msg_handle_base(){}
public:
	virtual void close() = 0;
	virtual size_t size() = 0;
	void check_lost(bool checkLost = true);
	const shared_bool& dead_sign();
private:
	void lost_msg();
protected:
	void set_actor(my_actor* hostActor);
	virtual void stop_waiting() = 0;
	virtual void throw_lost_exception() = 0;
protected:
	my_actor* _hostActor;
	shared_bool _closed;
	DEBUG_OPERATION(shared_strand _strand);
	bool _waiting : 1;
	bool _losted : 1;
	bool _checkLost : 1;
};

template <typename... ARGS>
class ActorMsgHandlePush_ : public msg_handle_base
{
	friend my_actor;
	friend MsgNotiferBase_<ARGS...>;
	typedef DstReceiverBase_<TYPE_PIPE(ARGS)...> dst_receiver;
protected:
	virtual void push_msg(std::tuple<TYPE_PIPE(ARGS)...>&&) = 0;
	virtual bool read_msg(dst_receiver& dst) = 0;
};

template <>
class ActorMsgHandlePush_<> : public msg_handle_base
{
	friend my_actor;
	friend MsgNotiferBase_<>;
	typedef DstReceiverBase_<> dst_receiver;
protected:
	virtual void push_msg() = 0;
	virtual bool read_msg() = 0;
};

template <typename... ARGS>
class MsgNotiferBase_
{
	typedef ActorMsgHandlePush_<ARGS...> MsgHandle;
protected:
	MsgNotiferBase_()
		:_msgHandle(NULL){}

	MsgNotiferBase_(MsgHandle* msgHandle, bool checkLost)
		:_msgHandle(msgHandle),
		_hostActor(ActorFunc_::shared_from_this(_msgHandle->_hostActor)),
		_closed(msgHandle->_closed)		
	{
		assert(msgHandle->_strand == ActorFunc_::self_strand(_hostActor.get()));
#ifdef ENABLE_CHECK_LOST
		if (checkLost)
		{
			_autoCheckLost = ActorFunc_::new_check_lost(ActorFunc_::self_strand(_hostActor.get()), msgHandle);
		}
#else
		assert(!checkLost);
#endif
	}
public:
	template <typename... Args>
	void operator()(Args&&... args) const
	{
		static_assert(sizeof...(ARGS) == sizeof...(Args), "");
		assert(!empty());
		if (!_closed)
		{
			typedef std::tuple<TYPE_PIPE(ARGS)...> args_tuple;
			if (ActorFunc_::self_strand(_hostActor.get())->running_in_this_thread())
			{
				_msgHandle->push_msg(args_tuple(std::forward<Args>(args)...));
			}
			else
			{
				ActorFunc_::self_strand(_hostActor.get())->post(std::bind([](actor_handle& hostActor, MsgHandle* msgHandle, shared_bool& closed, args_tuple& args)
				{
					if (!closed)
					{
						msgHandle->push_msg(std::move(args));
					}
				}, _hostActor, _msgHandle, _closed, args_tuple(std::forward<Args>(args)...)));
			}
		}
	}

	void operator()() const
	{
		static_assert(sizeof...(ARGS) == 0, "");
		assert(!empty());
		if (!_closed)
		{
			if (ActorFunc_::self_strand(_hostActor.get())->running_in_this_thread())
			{
				_msgHandle->push_msg();
			}
			else
			{
				ActorFunc_::self_strand(_hostActor.get())->post(std::bind([](actor_handle& hostActor, MsgHandle* msgHandle, shared_bool& closed)
				{
					if (!closed)
					{
						msgHandle->push_msg();
					}
				}, _hostActor, _msgHandle, _closed));
			}
		}
	}

	std::function<void(ARGS...)> case_func() const
	{
		return std::function<void(ARGS...)>(*this);
	}

	const actor_handle& dest_actor() const
	{
		return _hostActor;
	}

	bool empty() const
	{
		return !_msgHandle;
	}

	void clear()
	{
		_msgHandle = NULL;
		_hostActor.reset();
		_closed.reset();
#ifdef ENABLE_CHECK_LOST
		_autoCheckLost.reset();
#endif
	}

	operator bool() const
	{
		return !empty();
	}

	const shared_bool& dead_sign() const
	{
		return _closed;
	}
protected:
	MsgNotiferBase_(const MsgNotiferBase_<ARGS...>& s)
		:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed)
#ifdef ENABLE_CHECK_LOST
		, _autoCheckLost(s._autoCheckLost)
#endif
	{}

	MsgNotiferBase_(MsgNotiferBase_<ARGS...>&& s)
		:_msgHandle(s._msgHandle), _hostActor(std::move(s._hostActor)), _closed(std::move(s._closed))
#ifdef ENABLE_CHECK_LOST
		, _autoCheckLost(std::move(s._autoCheckLost))
#endif
	{
		s._msgHandle = NULL;
	}

	void operator =(const MsgNotiferBase_<ARGS...>& s)
	{
		_msgHandle = s._msgHandle;
		_hostActor = s._hostActor;
		_closed = s._closed;
#ifdef ENABLE_CHECK_LOST
		_autoCheckLost = s._autoCheckLost;
#endif
	}

	void operator =(MsgNotiferBase_<ARGS...>&& s)
	{
		_msgHandle = s._msgHandle;
		_hostActor = std::move(s._hostActor);
		_closed = std::move(s._closed);
#ifdef ENABLE_CHECK_LOST
		_autoCheckLost = std::move(s._autoCheckLost);
#endif
		s._msgHandle = NULL;
	}
private:
	MsgHandle* _msgHandle;
	actor_handle _hostActor;
	shared_bool _closed;
#ifdef ENABLE_CHECK_LOST
	std::shared_ptr<CheckLost_> _autoCheckLost;
#endif
};

template <typename... ARGS>
class msg_notifer : public MsgNotiferBase_<ARGS...>
{
	friend msg_handle<ARGS...>;
public:
	msg_notifer()	{}
private:
	msg_notifer(ActorMsgHandlePush_<ARGS...>* msgHandle, bool checkLost)
		:MsgNotiferBase_<ARGS...>(msgHandle, checkLost) {}
public:
	msg_notifer(const msg_notifer<ARGS...>& s)
		:MsgNotiferBase_<ARGS...>(s) {}

	msg_notifer(msg_notifer<ARGS...>&& s)
		:MsgNotiferBase_<ARGS...>(std::move(s)) {}

	void operator=(const msg_notifer<ARGS...>& s)
	{
		MsgNotiferBase_<ARGS...>::operator =(s);
	}

	void operator=(msg_notifer<ARGS...>&& s)
	{
		MsgNotiferBase_<ARGS...>::operator =(std::move(s));
	}
};

template <typename... ARGS>
class trig_notifer : public MsgNotiferBase_<ARGS...>
{
	friend trig_handle<ARGS...>;
public:
	trig_notifer() {}
private:
	trig_notifer(ActorMsgHandlePush_<ARGS...>* msgHandle, bool checkLost)
		:MsgNotiferBase_<ARGS...>(msgHandle, checkLost) {}
public:
	trig_notifer(const trig_notifer<ARGS...>& s)
		:MsgNotiferBase_<ARGS...>(s) {}

	trig_notifer(trig_notifer<ARGS...>&& s)
		:MsgNotiferBase_<ARGS...>(std::move(s)) {}

	void operator=(const trig_notifer<ARGS...>& s)
	{
		MsgNotiferBase_<ARGS...>::operator =(s);
	}

	void operator=(trig_notifer<ARGS...>&& s)
	{
		MsgNotiferBase_<ARGS...>::operator =(std::move(s));
	}
};

template <typename... ARGS>
class msg_handle : public ActorMsgHandlePush_<ARGS...>
{
	typedef ActorMsgHandlePush_<ARGS...> Parent;
	typedef std::tuple<TYPE_PIPE(ARGS)...> msg_type;
	typedef DstReceiverBase_<TYPE_PIPE(ARGS)...> dst_receiver;
	typedef msg_notifer<ARGS...> MsgNotifer;

	friend select_block_msg<ARGS...>;
#ifdef ENABLE_CHECK_LOST
	friend select_block_msg_check_lost<ARGS...>;
#endif
	friend my_actor;
public:
	struct lost_exception : ntf_lost_exception {};
public:
	msg_handle(size_t fixedSize = 16)
		:_msgBuff(fixedSize), _dstRec(NULL) {}

	~msg_handle()
	{
		close();
	}
private:
	MsgNotifer make_notifer(my_actor* hostActor, bool checkLost)
	{
		close();
		Parent::set_actor(hostActor);
		Parent::_closed = shared_bool::new_();
		return MsgNotifer(this, checkLost);
	}

	void push_msg(msg_type&& msg)
	{
		assert(Parent::_strand->running_in_this_thread());
		if (!ActorFunc_::is_quited(Parent::_hostActor))
		{
			if (Parent::_waiting)
			{
				Parent::_waiting = false;
				assert(_msgBuff.empty());
				assert(_dstRec);
				_dstRec->move_from(std::move(msg));
				_dstRec = NULL;
				ActorFunc_::pull_yield(Parent::_hostActor);
				return;
			}
			assert(_msgBuff.size() < _msgBuff.fixed_size());
			_msgBuff.push_back(std::move(msg));
		}
	}

	bool read_msg(dst_receiver& dst)
	{
		assert(Parent::_strand->running_in_this_thread());
		assert(!Parent::_closed.empty());
		assert(!Parent::_closed);
		if (!_msgBuff.empty())
		{
			dst.move_from(std::move(_msgBuff.front()));
			_msgBuff.pop_front();
			return true;
		}
		_dstRec = &dst;
		Parent::_waiting = true;
		return false;
	}

	void stop_waiting()
	{
		Parent::_waiting = false;
		_dstRec = NULL;
	}

	void throw_lost_exception()
	{
		throw lost_exception();
	}
public:
	void close()
	{
		if (!Parent::_closed.empty())
		{
			assert(Parent::_strand->running_in_this_thread());
			Parent::_closed = true;
			Parent::_closed.reset();
		}
		_dstRec = NULL;
		Parent::_waiting = false;
		Parent::_losted = false;
		Parent::_checkLost = false;
		_msgBuff.clear();
		Parent::_hostActor = NULL;
	}

	size_t size()
	{
		assert(Parent::_strand->running_in_this_thread());
		return _msgBuff.size();
	}
private:
	dst_receiver* _dstRec;
	msg_queue<msg_type> _msgBuff;
};

template <>
class msg_handle<> : public ActorMsgHandlePush_<>
{
	typedef ActorMsgHandlePush_<> Parent;
	typedef msg_notifer<> MsgNotifer;

	friend select_block_msg<>;
#ifdef ENABLE_CHECK_LOST
	friend select_block_msg_check_lost<>;
#endif
	friend my_actor;
public:
	struct lost_exception : ntf_lost_exception {};
public:
	~msg_handle()
	{
		close();
	}
private:
	MsgNotifer make_notifer(my_actor* hostActor, bool checkLost)
	{
		close();
		Parent::set_actor(hostActor);
		Parent::_closed = shared_bool::new_();
		return MsgNotifer(this, checkLost);
	}

	void push_msg()
	{
		assert(Parent::_strand->running_in_this_thread());
		if (!ActorFunc_::is_quited(Parent::_hostActor))
		{
			if (Parent::_waiting)
			{
				Parent::_waiting = false;
				if (_dstRec)
				{
					*_dstRec = true;
					_dstRec = NULL;
				}
				ActorFunc_::pull_yield(Parent::_hostActor);
				return;
			}
			_msgCount++;
		}
	}

	bool read_msg()
	{
		assert(Parent::_strand->running_in_this_thread());
		assert(!Parent::_closed.empty());
		assert(!Parent::_closed);
		assert(!_dstRec);
		if (_msgCount)
		{
			_msgCount--;
			return true;
		}
		Parent::_waiting = true;
		return false;
	}

	bool read_msg(bool& dst)
	{
		assert(Parent::_strand->running_in_this_thread());
		assert(!Parent::_closed.empty());
		assert(!Parent::_closed);
		if (_msgCount)
		{
			_msgCount--;
			dst = true;
			return true;
		}
		_dstRec = &dst;
		Parent::_waiting = true;
		return false;
	}

	void stop_waiting()
	{
		Parent::_waiting = false;
		_dstRec = NULL;
	}

	void throw_lost_exception()
	{
		throw lost_exception();
	}
public:
	void close()
	{
		if (!Parent::_closed.empty())
		{
			assert(Parent::_strand->running_in_this_thread());
			Parent::_closed = true;
			Parent::_closed.reset();
		}
		_dstRec = NULL;
		_msgCount = 0;
		Parent::_waiting = false;
		Parent::_losted = false;
		Parent::_checkLost = false;
		Parent::_hostActor = NULL;
	}

	size_t size()
	{
		assert(Parent::_strand->running_in_this_thread());
		return _msgCount;
	}
private:
	bool* _dstRec;
	size_t _msgCount;
};
//////////////////////////////////////////////////////////////////////////

template <typename... ARGS>
class trig_handle : public ActorMsgHandlePush_<ARGS...>
{
	typedef ActorMsgHandlePush_<ARGS...> Parent;
	typedef std::tuple<TYPE_PIPE(ARGS)...> msg_type;
	typedef DstReceiverBase_<TYPE_PIPE(ARGS)...> dst_receiver;
	typedef trig_notifer<ARGS...> MsgNotifer;

	friend select_block_trig<ARGS...>;
#ifdef ENABLE_CHECK_LOST
	friend select_block_trig_check_lost<ARGS...>;
#endif
	friend my_actor;
public:
	struct lost_exception : ntf_lost_exception {};
public:
	trig_handle()
		:_hasMsg(false), _dstRec(NULL) {}

	~trig_handle()
	{
		close();
	}
private:
	MsgNotifer make_notifer(my_actor* hostActor, bool checkLost)
	{
		close();
		Parent::set_actor(hostActor);
		Parent::_closed = shared_bool::new_();
		return MsgNotifer(this, checkLost);
	}

	void push_msg(msg_type&& msg)
	{
		assert(Parent::_strand->running_in_this_thread());
		Parent::_closed = true;
		if (!ActorFunc_::is_quited(Parent::_hostActor))
		{
			if (Parent::_waiting)
			{
				Parent::_waiting = false;
				assert(_dstRec);
				_dstRec->move_from(std::move(msg));
				_dstRec = NULL;
				ActorFunc_::pull_yield(Parent::_hostActor);
				return;
			}
			_hasMsg = true;
			new(_msgBuff)msg_type(std::move(msg));
		}
	}

	bool read_msg(dst_receiver& dst)
	{
		assert(Parent::_strand->running_in_this_thread());
		assert(!Parent::_closed.empty());
		if (_hasMsg)
		{
			_hasMsg = false;
			dst.move_from(std::move(*as_ptype<msg_type>(_msgBuff)));
			as_ptype<msg_type>(_msgBuff)->~msg_type();
			return true;
		}
		assert(!Parent::_closed);
		_dstRec = &dst;
		Parent::_waiting = true;
		return false;
	}

	void stop_waiting()
	{
		Parent::_waiting = false;
		_dstRec = NULL;
	}

	void throw_lost_exception()
	{
		throw lost_exception();
	}
public:
	void close()
	{
		if (!Parent::_closed.empty())
		{
			assert(Parent::_strand->running_in_this_thread());
			Parent::_closed = true;
			Parent::_closed.reset();
		}
		if (_hasMsg)
		{
			_hasMsg = false;
			as_ptype<msg_type>(_msgBuff)->~msg_type();
		}
		_dstRec = NULL;
		Parent::_waiting = false;
		Parent::_losted = false;
		Parent::_checkLost = false;
		Parent::_hostActor = NULL;
	}

	bool has()
	{
		assert(Parent::_strand->running_in_this_thread());
		return _hasMsg;
	}

	size_t size()
	{
		assert(Parent::_strand->running_in_this_thread());
		return _hasMsg ? 1 : 0;
	}
private:
	__space_align char _msgBuff[sizeof(msg_type)];
	dst_receiver* _dstRec;
	bool _hasMsg;
};

template <>
class trig_handle<> : public ActorMsgHandlePush_<>
{
	typedef ActorMsgHandlePush_<> Parent;
	typedef trig_notifer<> MsgNotifer;

	friend select_block_trig<>;
#ifdef ENABLE_CHECK_LOST
	friend select_block_trig_check_lost<>;
#endif
	friend my_actor;
public:
	struct lost_exception : ntf_lost_exception {};
public:
	trig_handle()
		:_hasMsg(false){}

	~trig_handle()
	{
		close();
	}
private:
	MsgNotifer make_notifer(my_actor* hostActor, bool checkLost)
	{
		close();
		Parent::set_actor(hostActor);
		Parent::_closed = shared_bool::new_();
		return MsgNotifer(this, checkLost);
	}

	void push_msg()
	{
		assert(Parent::_strand->running_in_this_thread());
		Parent::_closed = true;
		if (!ActorFunc_::is_quited(Parent::_hostActor))
		{
			if (Parent::_waiting)
			{
				Parent::_waiting = false;
				if (_dstRec)
				{
					*_dstRec = true;
					_dstRec = NULL;
				}
				ActorFunc_::pull_yield(Parent::_hostActor);
				return;
			}
			_hasMsg = true;
		}
	}

	bool read_msg()
	{
		assert(Parent::_strand->running_in_this_thread());
		assert(!Parent::_closed.empty());
		assert(!_dstRec);
		if (_hasMsg)
		{
			_hasMsg = false;
			return true;
		}
		assert(!Parent::_closed);
		Parent::_waiting = true;
		return false;
	}

	bool read_msg(bool& dst)
	{
		assert(Parent::_strand->running_in_this_thread());
		assert(!Parent::_closed.empty());
		if (_hasMsg)
		{
			_hasMsg = false;
			dst = true;
			return true;
		}
		assert(!Parent::_closed);
		_dstRec = &dst;
		Parent::_waiting = true;
		return false;
	}

	void stop_waiting()
	{
		Parent::_waiting = false;
		_dstRec = NULL;
	}

	void throw_lost_exception()
	{
		throw lost_exception();
	}
public:
	void close()
	{
		if (!Parent::_closed.empty())
		{
			assert(Parent::_strand->running_in_this_thread());
			Parent::_closed = true;
			Parent::_closed.reset();
		}
		_dstRec = NULL;
		_hasMsg = false;
		Parent::_waiting = false;
		Parent::_losted = false;
		Parent::_checkLost = false;
		Parent::_hostActor = NULL;
	}

	bool has()
	{
		assert(Parent::_strand->running_in_this_thread());
		return _hasMsg;
	}

	size_t size()
	{
		assert(Parent::_strand->running_in_this_thread());
		return _hasMsg ? 1 : 0;
	}
private:
	bool* _dstRec;
	bool _hasMsg;
};
//////////////////////////////////////////////////////////////////////////

template <typename... ARGS>
class MsgPump_;

template <typename... ARGS>
class MsgPool_;

template <typename... ARGS>
class post_actor_msg;

class MsgPumpBase_
{
	friend my_actor;
public:
	virtual ~MsgPumpBase_() {}
protected:
	my_actor* _hostActor;
};

template <typename... ARGS>
class MsgPump_ : public MsgPumpBase_
{
	typedef MsgPumpBase_ Parent;
	typedef std::tuple<TYPE_PIPE(ARGS)...> msg_type;
	typedef DstReceiverBase_<TYPE_PIPE(ARGS)...> dst_receiver;
	typedef MsgPool_<ARGS...> msg_pool_type;
	typedef typename msg_pool_type::pump_handler pump_handler;

	friend my_actor;
	friend MsgPool_<ARGS...>;
	friend select_block_pump<ARGS...>;
	friend select_block_pump_check_state<ARGS...>;
	friend pump_handler;
	friend msg_pump_handle<ARGS...>;
#ifdef ENABLE_CHECK_LOST
	friend select_block_pump_check_lost<ARGS...>;
#endif
	FRIEND_SHARED_PTR(MsgPump_<ARGS...>);
private:
	MsgPump_()
	{
		DEBUG_OPERATION(_closed = shared_bool::new_());
	}

	~MsgPump_()
	{
		assert(!_hasMsg);
		DEBUG_OPERATION(_closed = true);
	}
private:
	static std::shared_ptr<MsgPump_<ARGS...>> make(my_actor* hostActor, bool checkLost)
	{
#ifndef ENABLE_CHECK_LOST
		assert(!checkLost);
#endif
		std::shared_ptr<MsgPump_<ARGS...>> res = std::make_shared<MsgPump_<ARGS...>>();
		res->_weakThis = res;
		res->_hasMsg = false;
		res->_waiting = false;
		res->_checkDis = false;
		res->_losted = false;
		res->_waitConnect = false;
		res->_checkLost = checkLost;
		res->_pumpCount = 0;
		res->_dstRec = NULL;
		res->_hostActor = hostActor;
		res->_strand = ActorFunc_::self_strand(hostActor);
		return res;
	}

	void receiver(msg_type&& msg)
	{
		if (Parent::_hostActor)
		{
			assert(!_hasMsg);
			_pumpCount++;
			if (_dstRec)
			{
				_dstRec->move_from(std::move(msg));
				_dstRec = NULL;
				if (_waiting)
				{
					_waiting = false;
					_checkDis = false;
					ActorFunc_::pull_yield(_hostActor);
				}
				//read_msg时
			}
			else
			{//pump_msg超时结束后才接受到消息
				assert(!_waiting);
				_losted = false;
				_hasMsg = true;
				new (_msgSpace)msg_type(std::move(msg));
			}
		}
	}

	void _lost_msg()
	{
		_losted = true;
		_pumpCount++;
		if (_waiting && _checkLost)
		{
			_waiting = false;
			_checkDis = false;
			_dstRec = NULL;
			ActorFunc_::pull_yield(_hostActor);
		}
	}

	void lost_msg(actor_handle&& hostActor)
	{
		if (_strand->running_in_this_thread())
		{
			_lost_msg();
		}
		else
		{
			_strand->post(std::bind([](const actor_handle& hostActor, const std::shared_ptr<MsgPump_> sharedThis)
			{
				sharedThis->_lost_msg();
			}, std::move(hostActor), _weakThis.lock()));
		}
	}

	void receive_msg(msg_type&& msg, actor_handle&& hostActor)
	{
		if (_strand->running_in_this_thread())
		{
			receiver(std::move(msg));
		}
		else
		{
			_strand->post(std::bind([](const actor_handle& hostActor, const std::shared_ptr<MsgPump_> sharedThis, msg_type& msg)
			{
				sharedThis->receiver(std::move(msg));
			}, std::move(hostActor), _weakThis.lock(), std::move(msg)));
		}
	}

	bool read_msg(dst_receiver& dst)
	{
		assert(_strand->running_in_this_thread());
		assert(!_dstRec);
		assert(!_waiting);
		if (_hasMsg)
		{
			_hasMsg = false;
			dst.move_from(std::move(*as_ptype<msg_type>(_msgSpace)));
			as_ptype<msg_type>(_msgSpace)->~msg_type();
			return true;
		}
		_dstRec = &dst;
		if (!_pumpHandler.empty())
		{
			_pumpHandler.start_pump(_pumpCount);
			_waiting = !!_dstRec;
			return !_dstRec;
		}
		_waiting = true;
		return false;
	}

	bool try_read(dst_receiver& dst)
	{
		assert(_strand->running_in_this_thread());
		assert(!_dstRec);
		assert(!_waiting);
		assert(!dst.has());
		if (_hasMsg)
		{
			_hasMsg = false;
			dst.move_from(std::move(*as_ptype<msg_type>(_msgSpace)));
			((msg_type*)_msgSpace)->~msg_type();
			return true;
		}
#ifdef ENABLE_CHECK_LOST
		if (!_losted && !_pumpHandler.empty())
#else
		if (!_pumpHandler.empty())
#endif
		{
			bool wait = false;
			bool losted = false;
			if (_pumpHandler.try_pump(_hostActor, dst, _pumpCount, wait, losted))
			{
#ifdef ENABLE_CHECK_LOST
				if (wait)
				{
					_dstRec = &dst;
					_waiting = true;
					ActorFunc_::push_yield(_hostActor);
					assert(!_dstRec);
					assert(!_waiting);
					if (!_losted)
					{
						return true;
					}
					return false;
				}
				else if (!losted)
				{
					assert(!_dstRec);
					return true;
				}
				_losted = true;
#else
				if (wait)
				{
					_dstRec = &dst;
					_waiting = true;
					ActorFunc_::push_yield(_hostActor);
					assert(!_dstRec);
					assert(!_waiting);
				}
				return true;
#endif
			}
		}
		return false;
	}

	size_t size()
	{
		assert(_strand->running_in_this_thread());
		if (!_pumpHandler.empty())
		{
			return (_hasMsg ? 1 : 0) + _pumpHandler.size(_hostActor, _pumpCount);
		}
		return 0;
	}

	size_t snap_size()
	{
		assert(_strand->running_in_this_thread());
		if (!_pumpHandler.empty())
		{
			return (_hasMsg ? 1 : 0) + _pumpHandler.snap_size(_pumpCount);
		}
		return 0;
	}

	void stop_waiting()
	{
		_waiting = false;
		_waitConnect = false;
		_checkDis = false;
		_dstRec = NULL;
	}

	template <typename PumpHandler>
	void connect(PumpHandler&& pumpHandler)
	{
		assert(_strand->running_in_this_thread());
		assert(_hostActor);
		_pumpHandler = std::forward<PumpHandler>(pumpHandler);
		_pumpCount = 0;
		if (_waiting)
		{
			_pumpHandler.start_pump(_pumpCount);
		}
		else if (_waitConnect)
		{
			_waitConnect = false;
			ActorFunc_::pull_yield(_hostActor);
		}
	}

	void clear()
	{
		assert(_strand->running_in_this_thread());
		assert(_hostActor);
		_pumpHandler.clear();
		if (_hostActor && !ActorFunc_::is_quited(_hostActor) && _checkDis)
		{
			assert(_waiting);
			_waiting = false;
			_dstRec = NULL;
			ActorFunc_::pull_yield(_hostActor);
		}
	}

	void backflow(stack_obj<msg_type>& suck, bool& losted)
	{
		if (_hasMsg)
		{
			_hasMsg = false;
			suck.create(std::move(*as_ptype<msg_type>(_msgSpace)));
			as_ptype<msg_type>(_msgSpace)->~msg_type();
		}
		losted = _losted;
		_losted = false;
	}

	void close()
	{
		assert(_strand->running_in_this_thread());
		if (_hasMsg)
		{
			as_ptype<msg_type>(_msgSpace)->~msg_type();
		}
		_hasMsg = false;
		_dstRec = NULL;
		_pumpCount = 0;
		_waiting = false;
		_waitConnect = false;
		_checkDis = false;
		_losted = false;
		_checkLost = false;
		_pumpHandler.clear();
		Parent::_hostActor = NULL;
	}

	bool isDisconnected()
	{
		return _pumpHandler.empty();
	}
private:
	__space_align char _msgSpace[sizeof(msg_type)];
	std::weak_ptr<MsgPump_> _weakThis;
	pump_handler _pumpHandler;
	shared_strand _strand;
	dst_receiver* _dstRec;
	DEBUG_OPERATION(shared_bool _closed);
	unsigned char _pumpCount;
	bool _hasMsg : 1;
	bool _waiting : 1;
	bool _waitConnect : 1;
	bool _checkDis : 1;
	bool _checkLost : 1;
	bool _losted : 1;
};

class MsgPoolBase_
{
	friend my_actor;
	friend CheckPumpLost_;
protected:
	virtual void lost_msg(actor_handle&& hostActor) = 0;
protected:
#ifdef ENABLE_CHECK_LOST
	std::weak_ptr<CheckPumpLost_> _weakCheckLost;
#endif
};

template <typename... ARGS>
class MsgPool_ : public MsgPoolBase_
{
	typedef std::tuple<TYPE_PIPE(ARGS)...> msg_type;
	typedef DstReceiverBase_<TYPE_PIPE(ARGS)...> dst_receiver;
	typedef MsgPump_<ARGS...> msg_pump_type;
	typedef post_actor_msg<ARGS...> post_type;

	struct msg_pck 
	{
		msg_pck()
		: _isMsg(false) {}

		msg_pck(const msg_type& msg)
		{
			new(_msg)msg_type(msg);
			_isMsg = true;
		}

		msg_pck(msg_type&& msg)
		{
			new(_msg)msg_type(std::move(msg));
			_isMsg = true;
		}

		msg_pck(const msg_pck& s)
		{
			if (s._isMsg)
			{
				new(_msg)msg_type(s.get());
				_isMsg = true;
			}
			else
			{
				_isMsg = false;
			}
		}

		msg_pck(msg_pck&& s)
		{
			if (s._isMsg)
			{
				new(_msg)msg_type(std::move(s.get()));
				_isMsg = true;
			}
			else
			{
				_isMsg = false;
			}
		}

		~msg_pck()
		{
			if (_isMsg)
			{
				as_ptype<msg_type>(_msg)->~msg_type();
				_isMsg = false;
			}
		}

		msg_type& get()
		{
			assert(_isMsg);
			return *as_ptype<msg_type>(_msg);
		}

		__space_align char _msg[sizeof(msg_type)];
		bool _isMsg;
	private:
		void operator =(const msg_pck&) = delete;
	};

	struct pump_handler
	{
		pump_handler()
		{}

		pump_handler(const pump_handler& s)
		:_thisPool(s._thisPool), _msgPump(s._msgPump) {}

		pump_handler(pump_handler&& s)
			:_thisPool(std::move(s._thisPool)), _msgPump(std::move(s._msgPump)) {}

		void operator=(const pump_handler& s)
		{
			_thisPool = s._thisPool;
			_msgPump = s._msgPump;
		}

		void operator=(pump_handler&& s)
		{
			_thisPool = std::move(s._thisPool);
			_msgPump = std::move(s._msgPump);
		}

		void pump_msg(unsigned char pumpID, actor_handle&& hostActor)
		{
			assert(_msgPump == _thisPool->_msgPump);
			auto& msgBuff = _thisPool->_msgBuff;
			if (!_thisPool->_waiting)//上次取消息超时后取消了等待，此时取还没消息
			{
				if (pumpID == _thisPool->_sendCount)
				{
					if (!msgBuff.empty())
					{
						msg_pck mt_ = std::move(msgBuff.front());
						msgBuff.pop_front();
						_thisPool->_sendCount++;
#ifdef ENABLE_CHECK_LOST
						if (!mt_._isMsg)
						{
							_msgPump->lost_msg(std::move(hostActor));
						}
						else
#endif
						{
							_msgPump->receive_msg(std::move(mt_.get()), std::move(hostActor));
						}
					}
					else
					{
						_thisPool->_waiting = true;
					}
				}
				else
				{//上次消息没取到，重新取，但实际中间已经post出去了
					assert(!_thisPool->_waiting);
					assert(pumpID + 1 == _thisPool->_sendCount);
				}
			}
			else
			{
				assert(msgBuff.empty());
				assert(pumpID == _thisPool->_sendCount);
			}
		}

		void start_pump(unsigned char pumpID)
		{
			assert(_thisPool);
			if (_thisPool->_strand->running_in_this_thread())
			{
				if (_msgPump == _thisPool->_msgPump)
				{
					pump_msg(pumpID, _msgPump->_hostActor->shared_from_this());
				}
			}
			else
			{
				post_pump(pumpID);
			}
		}

		bool try_pump(my_actor* host, dst_receiver& dst, unsigned char pumpID, bool& wait, bool& losted)
		{
			assert(_thisPool);
			return ActorFunc_::send<bool>(host, _thisPool->_strand, std::bind([&dst, &wait, &losted, pumpID](pump_handler& pump)->bool
			{
				bool ok = false;
				auto& thisPool_ = pump._thisPool;
				if (pump._msgPump == thisPool_->_msgPump)
				{
					auto& msgBuff = thisPool_->_msgBuff;
					if (pumpID == thisPool_->_sendCount)
					{
						if (!msgBuff.empty())
						{
#ifdef ENABLE_CHECK_LOST
							if (!msgBuff.front()._isMsg)
							{
								msgBuff.pop_front();
								losted = true;
								ok = true;
							} 
							else
#endif
							{
								dst.move_from(std::move(msgBuff.front().get()));
								msgBuff.pop_front();
								ok = true;
							}
						}
					}
					else
					{//上次消息没取到，重新取，但实际中间已经post出去了
						assert(!thisPool_->_waiting);
						assert(pumpID + 1 == thisPool_->_sendCount);
						wait = true;
						ok = true;
					}
				}
				return ok;
			}, *this));
		}

		size_t size(my_actor* host, unsigned char pumpID)
		{
			assert(_thisPool);
			return ActorFunc_::send<size_t>(host, _thisPool->_strand, std::bind([pumpID](pump_handler& pump)->size_t
			{
				auto& thisPool_ = pump._thisPool;
				if (pump._msgPump == thisPool_->_msgPump)
				{
					auto& msgBuff = thisPool_->_msgBuff;
					if (pumpID == thisPool_->_sendCount)
					{
						return msgBuff.size();
					}
					else
					{
						return msgBuff.size() + 1;
					}
				}
				return 0;
			}, *this));
		}

		size_t snap_size(unsigned char pumpID)
		{
			assert(_thisPool);
			if (_thisPool->_strand->running_in_this_thread())
			{
				if (_msgPump == _thisPool->_msgPump)
				{
					auto& msgBuff = _thisPool->_msgBuff;
					if (pumpID == _thisPool->_sendCount)
					{
						return msgBuff.size();
					}
					else
					{
						return msgBuff.size() + 1;
					}
				}
				return 0;
			}
			return _thisPool->_msgBuff.size();
		}

		void post_pump(unsigned char pumpID)
		{
			assert(!empty());
			_thisPool->_strand->post(std::bind([pumpID](pump_handler& pump)
			{
				if (pump._msgPump == pump._thisPool->_msgPump)
				{
					pump.pump_msg(pumpID, pump._msgPump->_hostActor->shared_from_this());
				}
			}, *this));
		}

		bool empty()
		{
			return !_thisPool;
		}

		void clear()
		{
			_thisPool.reset();
			_msgPump.reset();
		}

		std::shared_ptr<MsgPool_> _thisPool;
		std::shared_ptr<msg_pump_type> _msgPump;
	};

	friend my_actor;
	friend msg_pump_type;
	friend post_type;
	FRIEND_SHARED_PTR(MsgPool_<ARGS...>);
private:
	MsgPool_(size_t fixedSize)
		:_msgBuff(fixedSize)
	{

	}

	~MsgPool_()
	{

	}
private:
	static std::shared_ptr<MsgPool_<ARGS...>> make(const shared_strand& strand, size_t fixedSize)
	{
		std::shared_ptr<MsgPool_<ARGS...>> res = std::make_shared<MsgPool_<ARGS...>>(fixedSize);
		res->_weakThis = res;
		res->_strand = strand;
		res->_waiting = false;
		res->_closed = false;
		res->_sendCount = 0;
		return res;
	}

	void send_msg(msg_type&& mt, actor_handle&& hostActor)
	{
		if (_closed) return;

		if (_waiting)
		{
			_waiting = false;
			assert(_msgPump);
			assert(_msgBuff.empty());
			_sendCount++;
			_msgPump->receive_msg(std::move(mt), std::move(hostActor));
		}
		else
		{
			assert(_msgBuff.size() < _msgBuff.fixed_size());
			_msgBuff.push_back(std::move(mt));
		}
	}

	void push_msg(msg_type&& mt, const actor_handle& hostActor)
	{
		if (_closed) return;

		if (_strand->running_in_this_thread())
		{
			send_msg(std::move(mt), ActorFunc_::shared_from_this(hostActor.get()));
		}
		else
		{
			_strand->post(std::bind([](actor_handle& hostActor, const std::shared_ptr<MsgPool_>& sharedThis, msg_type& msg)
			{
				sharedThis->send_msg(std::move(msg), std::move(hostActor));
			}, hostActor, _weakThis.lock(), std::move(mt)));
		}
	}

	void _lost_msg(actor_handle&& hostActor)
	{
		if (_closed) return;

		if (_waiting)
		{
			_waiting = false;
			assert(_msgPump);
			assert(_msgBuff.empty());
			_sendCount++;
			_msgPump->lost_msg(std::move(hostActor));
		}
		else
		{
			assert(_msgBuff.size() < _msgBuff.fixed_size());
			_msgBuff.push_back(msg_pck());
		}
	}

	void lost_msg(actor_handle&& hostActor)
	{
		if (!_closed)
		{
			_strand->try_tick(std::bind([](actor_handle& hostActor, const std::shared_ptr<MsgPool_>& sharedThis)
			{
				sharedThis->_lost_msg(std::move(hostActor));
			}, std::move(hostActor), _weakThis.lock()));
		}
	}

	pump_handler connect_pump(const std::shared_ptr<msg_pump_type>& msgPump)
	{
		assert(msgPump);
		assert(_strand->running_in_this_thread());
		_msgPump = msgPump;
		pump_handler compHandler;
		compHandler._thisPool = _weakThis.lock();
		compHandler._msgPump = msgPump;
		_sendCount = 0;
		_waiting = false;
		return compHandler;
	}

	void disconnect()
	{
		assert(_strand->running_in_this_thread());
		_msgPump.reset();
		_waiting = false;
	}

	void expand_fixed(size_t fixedSize)
	{
		assert(_strand->running_in_this_thread());
		_msgBuff.expand_fixed(fixedSize);
	}

	void backflow(stack_obj<msg_type>& suck)
	{
		assert(_strand->running_in_this_thread());
		if (suck.has())
		{
			_msgBuff.push_front(std::move(suck.get()));
		}
		else
		{
			_msgBuff.push_front(msg_pck());
		}
	}
private:
	std::weak_ptr<MsgPool_> _weakThis;
	shared_strand _strand;
	std::shared_ptr<msg_pump_type> _msgPump;
	msg_queue<msg_pck> _msgBuff;
	unsigned char _sendCount;
	bool _waiting : 1;
	bool _closed : 1;
};

class MsgPumpVoid_;

struct VoidBuffer_ 
{
	void push_back();
	void pop_front();
	void push_front();
	void push_lost_back();
	void push_lost_front();
	bool front_is_lost_msg();
	bool back_is_lost_msg();
	size_t length();
	bool empty();
	msg_queue<int> _buff;
	size_t _length = 0;
};

class MsgPoolVoid_ :public MsgPoolBase_
{
	typedef post_actor_msg<> post_type;
	typedef void msg_type;
	typedef MsgPumpVoid_ msg_pump_type;

	struct pump_handler
	{
		pump_handler();
		pump_handler(const pump_handler& s);
		pump_handler(pump_handler&& s);
		void operator=(const pump_handler& s);
		void operator=(pump_handler&& s);

		void start_pump(unsigned char pumpID);
		void pump_msg(unsigned char pumpID, actor_handle&& hostActor);
		bool try_pump(my_actor* host, unsigned char pumpID, bool& wait, bool& losted);
		size_t size(my_actor* host, unsigned char pumpID);
		size_t snap_size(unsigned char pumpID);
		void post_pump(unsigned char pumpID);
		bool empty();
		bool same_strand();
		void clear();

		std::shared_ptr<MsgPoolVoid_> _thisPool;
		std::shared_ptr<msg_pump_type> _msgPump;
	};

	friend my_actor;
	friend MsgPumpVoid_;
	friend post_type;
protected:
	MsgPoolVoid_(const shared_strand& strand);
	virtual ~MsgPoolVoid_();
protected:
	void send_msg(actor_handle&& hostActor);
	void push_msg(const actor_handle& hostActor);
	void lost_msg(actor_handle&& hostActor);
	void _lost_msg(actor_handle&& hostActor);
	pump_handler connect_pump(const std::shared_ptr<msg_pump_type>& msgPump);
	void disconnect();
	void expand_fixed(size_t fixedSize){};
	void backflow(stack_obj<msg_type>& suck);
protected:
	std::weak_ptr<MsgPoolVoid_> _weakThis;
	shared_strand _strand;
	std::shared_ptr<msg_pump_type> _msgPump;
	VoidBuffer_ _msgBuff;
	unsigned char _sendCount;
	bool _waiting : 1;
	bool _closed : 1;
};

class MsgPumpVoid_ : public MsgPumpBase_
{
	typedef MsgPoolVoid_ msg_pool_type;
	typedef void msg_type;
	typedef MsgPoolVoid_::pump_handler pump_handler;

	friend my_actor;
	friend MsgPoolVoid_;
	friend select_block_pump<>;
	friend select_block_pump_check_state<>;
	friend pump_handler;
#ifdef ENABLE_CHECK_LOST
	friend select_block_pump_check_lost<>;
#endif
protected:
	MsgPumpVoid_(my_actor* hostActor, bool checkLost);
	virtual ~MsgPumpVoid_();
protected:
	void receiver();
	void _lost_msg();
	void lost_msg(actor_handle&& hostActor);
	void receive_msg(actor_handle&& hostActor);
	bool read_msg();
	bool read_msg(bool& dst);
	bool try_read();
	size_t size();
	size_t snap_size();
	void stop_waiting();
	void connect(const pump_handler& pumpHandler);
	void clear();
	void close();
	void backflow(stack_obj<msg_type>& suck, bool& losted);
	bool isDisconnected();
protected:
	std::weak_ptr<MsgPumpVoid_> _weakThis;
	pump_handler _pumpHandler;
	shared_strand _strand;
	bool* _dstRec;
	unsigned char _pumpCount;
	bool _waiting : 1;
	bool _waitConnect : 1;
	bool _hasMsg : 1;
	bool _checkDis : 1;
	bool _checkLost : 1;
	bool _losted : 1;
};

template <>
class MsgPool_<> : public MsgPoolVoid_
{
	friend my_actor;
	FRIEND_SHARED_PTR(MsgPool_<>);
private:
	typedef std::shared_ptr<MsgPool_> handle;

	MsgPool_(const shared_strand& strand)
		:MsgPoolVoid_(strand){}

	~MsgPool_(){}

	static handle make(const shared_strand& strand, size_t fixedSize)
	{
		handle res(new MsgPool_(strand));
		res->_weakThis = res;
		return res;
	}
};

template <>
class MsgPump_<> : public MsgPumpVoid_
{
	friend my_actor;
	friend select_block_pump<>;
	friend select_block_pump_check_state<>;
	friend msg_pump_handle<>;
#ifdef ENALBE_CHECK_LOST
	friend select_block_pump_check_lost<>;
#endif
	FRIEND_SHARED_PTR(MsgPump_<>);
public:
	typedef MsgPump_* handle;
private:
	MsgPump_(my_actor* hostActor, bool checkLost)
		:MsgPumpVoid_(hostActor, checkLost)
	{
		DEBUG_OPERATION(_closed = shared_bool::new_());
	}

	~MsgPump_()
	{
		DEBUG_OPERATION(_closed = true);
	}

	static std::shared_ptr<MsgPump_> make(my_actor* hostActor, bool checkLost)
	{
		std::shared_ptr<MsgPump_> res = std::make_shared<MsgPump_>(hostActor, checkLost);
		res->_weakThis = res;
		return res;
	}

	DEBUG_OPERATION(shared_bool _closed);
};

template <typename... ARGS>
class msg_pump_handle
{
	friend my_actor;
	friend select_block_pump<ARGS...>;
	friend select_block_pump_check_state<ARGS...>;
#ifdef ENABLE_CHECK_LOST
	friend select_block_pump_check_lost<ARGS...>;
#endif

	typedef MsgPump_<ARGS...> pump;
public:
	struct lost_exception : public ntf_lost_exception
	{
		lost_exception(int id)
		:_id(id) {}

		int _id;
	};

	struct pump_disconnected : public pump_disconnected_exception
	{
		pump_disconnected(int id)
		:_id(id) {}

		int _id;
	};

	msg_pump_handle()
		:_handle(NULL), _id(0) {}

	int get_id() const
	{
		assert(_handle && _handle->_strand->running_in_this_thread());
		return _id;
	}

	void check_lost(bool checkLost = true)
	{
		assert(!_handle || _handle->_strand->running_in_this_thread());
#ifndef ENABLE_CHECK_LOST
		assert(!checkLost);
#endif
		_handle->_checkLost = checkLost;
	}

	void reset_lost()
	{
		assert(!_handle || _handle->_strand->running_in_this_thread());
		_handle->_losted = false;
	}

	void reset()
	{
		assert(!_handle || _handle->_strand->running_in_this_thread());
		_handle = NULL;
		_id = 0;
	}

	bool empty() const
	{
		assert(!_handle || _handle->_strand->running_in_this_thread());
		return !_handle;
	}

	bool is_connected() const
	{
		assert(_handle && _handle->_strand->running_in_this_thread());
		return !_handle->isDisconnected();
	}

	operator bool() const
	{
		assert(!_handle || _handle->_strand->running_in_this_thread());
		return !!_handle;
	}
private:
	pump* get() const
	{
		assert(!_closed);
		return _handle;
	}

#if (_DEBUG || DEBUG)
	bool check_closed() const
	{
		return _closed.empty() || _closed;
	}
#endif

	DEBUG_OPERATION(shared_bool _closed);
	pump* _handle;
	int _id;
};

template <typename... ARGS>
class post_actor_msg
{
	typedef MsgPool_<ARGS...> msg_pool_type;
public:
	post_actor_msg(){}
#ifdef ENABLE_CHECK_LOST
	template <typename ActorHandle, typename CheckPumpLost>
	post_actor_msg(const std::shared_ptr<msg_pool_type>& msgPool, ActorHandle&& hostActor, CheckPumpLost&& autoCheckLost)
		:_msgPool(msgPool), _hostActor(std::forward<ActorHandle>(hostActor)), _autoCheckLost(std::forward<CheckPumpLost>(autoCheckLost)) {}
#else
	template <typename ActorHandle>
	post_actor_msg(const std::shared_ptr<msg_pool_type>& msgPool, ActorHandle&& hostActor)
		:_msgPool(msgPool), _hostActor(std::forward<ActorHandle>(hostActor)) {}
#endif
	post_actor_msg(const post_actor_msg<ARGS...>& s)
		:_hostActor(s._hostActor), _msgPool(s._msgPool)
#ifdef ENABLE_CHECK_LOST
		, _autoCheckLost(s._autoCheckLost)
#endif
	{}

	post_actor_msg(post_actor_msg<ARGS...>&& s)
		:_hostActor(std::move(s._hostActor)), _msgPool(std::move(s._msgPool))
#ifdef ENABLE_CHECK_LOST
		, _autoCheckLost(std::move(s._autoCheckLost))
#endif
	{}

	void operator =(const post_actor_msg<ARGS...>& s)
	{
		_hostActor = s._hostActor;
		_msgPool = s._msgPool;
#ifdef ENABLE_CHECK_LOST
		_autoCheckLost = s._autoCheckLost;
#endif
	}

	void operator =(post_actor_msg<ARGS...>&& s)
	{
		_hostActor = std::move(s._hostActor);
		_msgPool = std::move(s._msgPool);
#ifdef ENABLE_CHECK_LOST
		_autoCheckLost = std::move(s._autoCheckLost);
#endif
	}
public:
	template <typename... Args>
	void operator()(Args&&... args) const
	{
		static_assert(sizeof...(ARGS) == sizeof...(Args), "");
		assert(!empty());
		_msgPool->push_msg(std::tuple<TYPE_PIPE(ARGS)...>(std::forward<Args>(args)...), _hostActor);
	}

	void operator()() const
	{
		static_assert(sizeof...(ARGS) == 0, "");
		assert(!empty());
		_msgPool->push_msg(_hostActor);
	}

	std::function<void(ARGS...)> case_func() const
	{
		return std::function<void(ARGS...)>(*this);
	}

	bool empty() const
	{
		return !_msgPool;
	}

	void clear()
	{
#ifdef ENABLE_CHECK_LOST
		_autoCheckLost.reset();//必须在_msgPool.reset()上面
#endif
		_hostActor.reset();
		_msgPool.reset();
	}

	operator bool() const
	{
		return !empty();
	}

	const actor_handle& dest_actor() const
	{
		return _hostActor;
	}
private:
	actor_handle _hostActor;
	std::shared_ptr<msg_pool_type> _msgPool;
#ifdef ENABLE_CHECK_LOST
	std::shared_ptr<CheckPumpLost_> _autoCheckLost;//必须在_msgPool下面
#endif
};
//////////////////////////////////////////////////////////////////////////

class MutexBlock_
{
	friend my_actor;
private:
	virtual bool ready() = 0;
	virtual void cancel() = 0;
	virtual bool go_run(bool& isRun) = 0;
	virtual size_t snap_id() = 0;
	virtual long long host_id() = 0;
	virtual void check_lost() = 0;
protected:
	MutexBlock_() {}
	long long actor_id(my_actor* host);
	NONE_COPY(MutexBlock_);
};

#define __MUTEX_BLOCK_HANDLER_WRAP(__dst__, __src__, __host__)  FUNCTION_ALLOCATOR(__dst__, __src__, (reusable_alloc<>(ActorFunc_::reu_mem(__host__))))

/*!
@brief msg_handle消息互斥执行块
*/
template <typename... ARGS>
class select_block_msg : public MutexBlock_
{
	typedef msg_handle<ARGS...> MsgHandle;
	typedef DstReceiverBuff_<ARGS...> dst_receiver;

	friend my_actor;
public:
	template <typename Handler>
	select_block_msg(MsgHandle& msgHandle, Handler&& handler)
		:_msgHandle(msgHandle), __MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor) {}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_msg(select_block_msg&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler))
	{
		assert(false);
		assert(!_msgBuff.has());
	}
#endif
private:
	bool ready()
	{
		assert(!_msgBuff.has());
		return _msgHandle.read_msg(_msgBuff);
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
	}

	void check_lost()
	{
		if (_msgHandle._checkLost && _msgHandle._losted)
		{
			throw typename msg_handle<ARGS...>::lost_exception();
		}
	}

	bool go_run(bool& isRun)
	{
		if (_msgBuff.has())
		{
			isRun = true;
			BREAK_OF_SCOPE_EXEC(_msgBuff.clear());
			return tuple_invoke<bool>(_handler, std::move(_msgBuff._dstBuff.get()));
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool(ARGS...)> _handler;
	dst_receiver _msgBuff;
};

/*!
@brief trig_handle消息互斥执行块
*/
template <typename... ARGS>
class select_block_trig : public MutexBlock_
{
	typedef trig_handle<ARGS...> MsgHandle;
	typedef DstReceiverBuff_<ARGS...> dst_receiver;

	friend my_actor;
public:
	template <typename Handler>
	select_block_trig(MsgHandle& msgHandle, Handler&& handler)
		:_msgHandle(msgHandle), __MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor) {}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_trig(select_block_trig&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler))
	{
		assert(false);
		assert(!_msgBuff.has());
	}
#endif
private:
	bool ready()
	{
		assert(!_msgBuff.has());
		return _msgHandle.read_msg(_msgBuff);
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
	}

	void check_lost()
	{
		if (_msgHandle._checkLost && _msgHandle._losted)
		{
			throw typename trig_handle<ARGS...>::lost_exception();
		}
	}

	bool go_run(bool& isRun)
	{
		if (_msgBuff.has())
		{
			isRun = true;
			BREAK_OF_SCOPE_EXEC(_msgBuff.clear());
			return tuple_invoke<bool>(_handler, std::move(_msgBuff._dstBuff.get()));
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool(ARGS...)> _handler;
	dst_receiver _msgBuff;
};

/*!
@brief msg_pump消息互斥执行块
*/
template <typename... ARGS>
class select_block_pump : public MutexBlock_
{
	typedef msg_pump_handle<ARGS...> pump_handle;
	typedef DstReceiverBuff_<ARGS...> dst_receiver;

	friend my_actor;
public:
	template <typename Handler>
	select_block_pump(my_actor* host, Handler&& handler, bool checkLost = false)
		:select_block_pump(0, host, std::forward<Handler>(handler), checkLost) {}

	template <typename Handler>
	select_block_pump(const int id, my_actor* host, Handler&& handler, bool checkLost = false)
		: select_block_pump(ActorFunc_::connect_msg_pump<ARGS...>(id, host, checkLost), std::forward<Handler>(handler)) {}

	template <typename Handler>
	select_block_pump(const pump_handle& pump, Handler&& handler)
		: _msgHandle(pump), __MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), pump.get()->_hostActor) {}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_pump(select_block_pump&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler))
	{
		assert(false);
		assert(!_msgBuff.has());
	}
#endif
private:
	bool ready()
	{
		assert(!_msgBuff.has());
		assert(!_msgHandle.check_closed());
		return _msgHandle._handle->read_msg(_msgBuff);
	}

	void cancel()
	{
		assert(!_msgHandle.check_closed());
		_msgHandle._handle->stop_waiting();
	}

	void check_lost()
	{
		auto* t = _msgHandle.get();
		if (t->_checkLost && t->_losted)
		{
			throw typename msg_pump_handle<ARGS...>::lost_exception(_msgHandle.get_id());
		}
	}

	bool go_run(bool& isRun)
	{
		assert(!_msgHandle.check_closed());
		if (_msgBuff.has())
		{
			isRun = true;
			BREAK_OF_SCOPE_EXEC(_msgBuff.clear());
			return tuple_invoke<bool>(_handler, std::move(_msgBuff._dstBuff.get()));
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		assert(!_msgHandle.check_closed());
		return (size_t)_msgHandle._handle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle.get()->_hostActor);
	}
private:
	pump_handle _msgHandle;
	std::function<bool(ARGS...)> _handler;
	dst_receiver _msgBuff;
};

template <>
class select_block_msg<> : public MutexBlock_
{
	typedef msg_handle<> MsgHandle;

	friend my_actor;
public:
	template <typename Handler>
	select_block_msg(MsgHandle& msgHandle, Handler&& handler)
		:_msgHandle(msgHandle), _has(false), __MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor) {}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_msg(select_block_msg&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler))
	{
		assert(false);
		assert(!_has);
	}
#endif
private:
	bool ready()
	{
		assert(!_has);
		return _msgHandle.read_msg(_has);
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
	}

	void check_lost()
	{
		if (_msgHandle._checkLost && _msgHandle._losted)
		{
			throw msg_handle<>::lost_exception();
		}
	}

	bool go_run(bool& isRun)
	{
		if (_has)
		{
			_has = false;
			isRun = true;
			return _handler();
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool()> _handler;
	bool _has;
};

template <>
class select_block_trig<> : public MutexBlock_
{
	typedef trig_handle<> MsgHandle;

	friend my_actor;
public:
	template <typename Handler>
	select_block_trig(MsgHandle& msgHandle, Handler&& handler)
		:_msgHandle(msgHandle), _has(false), __MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor) {}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_trig(select_block_trig&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler))
	{
		assert(false);
		assert(!_has);
	}
#endif
private:
	bool ready()
	{
		assert(!_has);
		return _msgHandle.read_msg(_has);
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
	}

	void check_lost()
	{
		if (_msgHandle._checkLost && _msgHandle._losted)
		{
			throw trig_handle<>::lost_exception();
		}
	}

	bool go_run(bool& isRun)
	{
		if (_has)
		{
			_has = false;
			isRun = true;
			return _handler();
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool()> _handler;
	bool _has;
};

template <>
class select_block_pump<> : public MutexBlock_
{
	typedef msg_pump_handle<> pump_handle;

	friend my_actor;
public:
	template <typename Handler>
	select_block_pump(my_actor* host, Handler&& handler, bool checkLost = false)
		:select_block_pump(0, host, std::forward<Handler>(handler), checkLost) {}

	template <typename Handler>
	select_block_pump(const int id, my_actor* host, Handler&& handler, bool checkLost = false)
		: select_block_pump(ActorFunc_::connect_msg_pump(id, host, checkLost), std::forward<Handler>(handler)) {}

	template <typename Handler>
	select_block_pump(const pump_handle& pump, Handler&& handler)
		: _msgHandle(pump), _has(false), __MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), pump.get()->_hostActor) {}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_pump(select_block_pump&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler))
	{
		assert(false);
		assert(!_has);
	}
#endif
private:
	bool ready()
	{
		assert(!_has);
		assert(!_msgHandle.check_closed());
		return _msgHandle._handle->read_msg(_has);
	}

	void cancel()
	{
		assert(!_msgHandle.check_closed());
		_msgHandle._handle->stop_waiting();
	}

	void check_lost()
	{
		auto* t = _msgHandle.get();
		if (t->_checkLost && t->_losted)
		{
			throw msg_pump_handle<>::lost_exception(_msgHandle.get_id());
		}
	}

	bool go_run(bool& isRun)
	{
		assert(!_msgHandle.check_closed());
		if (_has)
		{
			_has = false;
			isRun = true;
			return _handler();
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		assert(!_msgHandle.check_closed());
		return (size_t)_msgHandle._handle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle.get()->_hostActor);
	}
private:
	pump_handle _msgHandle;
	std::function<bool()> _handler;
	bool _has;
};

class select_block_quit : public MutexBlock_
{
	friend my_actor;
public:
	template <typename Handler>
	select_block_quit(my_actor* host, Handler&& handler)
		:_host(host), _quitHandler(std::forward<Handler>(handler)), _quitNtfed(false)
	{
		check_lock_quit();
	}
private:
	bool ready();
	void cancel();
	void check_lost();
	bool go_run(bool& isRun);
	size_t snap_id();
	long long host_id();
	void check_lock_quit();
private:
	my_actor* _host;
	std::function<bool()> _quitHandler;
	bool _quitNtfed;
};

class select_block_sign : public MutexBlock_
{
	friend my_actor;
public:
	template <typename Handler>
	select_block_sign(my_actor* host, int id, Handler&& handler)
		:_host(host), _signHandler(std::forward<Handler>(handler)), _mask((size_t)1 << id), _signNtfed(false)
	{
		assert(id >= 0 && id < 8 * (int)sizeof(void*));
	}
private:
	bool ready();
	void cancel();
	void check_lost();
	bool go_run(bool& isRun);
	size_t snap_id();
	long long host_id();
private:
	my_actor* _host;
	std::function<bool()> _signHandler;
	size_t _mask;
	bool _signNtfed;
};
//////////////////////////////////////////////////////////////////////////

template <typename... ARGS>
class select_block_pump_check_state : public MutexBlock_
{
	typedef msg_pump_handle<ARGS...> pump_handle;
	typedef DstReceiverBuff_<ARGS...> dst_receiver;

	friend my_actor;
public:
	template <typename Handler, typename StateHandler>
	select_block_pump_check_state(my_actor* host, Handler&& handler, StateHandler&& stateHandler)
		:select_block_pump_check_state(0, host, std::forward<Handler>(handler), std::forward<StateHandler>(stateHandler)) {}

	template <typename Handler, typename StateHandler>
	select_block_pump_check_state(const int id, my_actor* host, Handler&& handler, StateHandler&& stateHandler)
		: select_block_pump_check_state(ActorFunc_::connect_msg_pump<ARGS...>(id, host, true), std::forward<Handler>(handler), std::forward<StateHandler>(stateHandler)) {}

	template <typename Handler, typename StateHandler>
	select_block_pump_check_state(const pump_handle& pump, Handler&& handler, StateHandler&& stateHandler)
		:_msgHandle(pump), _readySign(0), _connected(false), _disconnected(false), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), pump.get()->_hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_stateHandler, std::forward<StateHandler>(stateHandler), pump.get()->_hostActor)
	{
	}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_pump_check_state(select_block_pump_check_state&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _stateHandler(std::move(s._stateHandler)),
		_readySign(s._readySign), _connected(s._connected), _disconnected(s._disconnected), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_msgBuff.has());
	}
#endif
private:
	bool ready()
	{
		assert(!_msgBuff.has());
		assert(!_msgHandle.check_closed());
		_readySign = 1;
		if (!_connected)
		{
			if (_msgHandle.is_connected())
			{
				return true;
			}
			_msgHandle._handle->_waitConnect = true;
			return false;
		}
		if (_msgHandle._handle->read_msg(_msgBuff) || is_losted())
		{
			return true;
		}
		_msgHandle._handle->_checkDis = true;
		return _msgHandle._handle->isDisconnected();
	}

	void cancel()
	{
		assert(!_msgHandle.check_closed());
		_msgHandle._handle->stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle.get()->_checkLost);
		return _msgHandle.get()->_losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		assert(!_msgHandle.check_closed());
		if (0 == _readySign)
		{
			if (!_connected && _msgHandle.is_connected())
			{
				_connected = true;
				_disconnected = false;
				isRun = true;
				return _stateHandler(pump_check_state::pump_connected);
			}
			else if (_msgBuff.has())
			{
				isRun = true;
				_lostNtfed = false;
				BREAK_OF_SCOPE_EXEC(_msgBuff.clear());
				return tuple_invoke<bool>(_handler, std::move(_msgBuff._dstBuff.get()));
			}
			else if (_connected && !_disconnected && !_msgHandle.is_connected())
			{
				_disconnected = true;
				_connected = false;
				isRun = true;
				return _stateHandler(pump_check_state::pump_disconnted);
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _stateHandler(pump_check_state::pump_ntf_losted);
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		assert(!_msgHandle.check_closed());
		return (size_t)_msgHandle._handle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle.get()->_hostActor);
	}
private:
	pump_handle _msgHandle;
	std::function<bool(ARGS...)> _handler;
	std::function<bool(pump_check_state)> _stateHandler;
	dst_receiver _msgBuff;
	int _readySign;
	bool _connected;
	bool _disconnected;
	bool _lostNtfed;
};

template <>
class select_block_pump_check_state<> : public MutexBlock_
{
	typedef msg_pump_handle<> pump_handle;

	friend my_actor;
public:
	template <typename Handler, typename StateHandler>
	select_block_pump_check_state(my_actor* host, Handler&& handler, StateHandler&& stateHandler)
		:select_block_pump_check_state(0, host, std::forward<Handler>(handler), std::forward<StateHandler>(stateHandler)) {}

	template <typename Handler, typename StateHandler>
	select_block_pump_check_state(const int id, my_actor* host, Handler&& handler, StateHandler&& stateHandler)
		: select_block_pump_check_state(ActorFunc_::connect_msg_pump<>(id, host, true), std::forward<Handler>(handler), std::forward<StateHandler>(stateHandler)) {}

	template <typename Handler, typename StateHandler>
	select_block_pump_check_state(const pump_handle& pump, Handler&& handler, StateHandler&& stateHandler)
		:_msgHandle(pump), _readySign(0), _has(false), _connected(false), _disconnected(false), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), pump.get()->_hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_stateHandler, std::forward<StateHandler>(stateHandler), pump.get()->_hostActor)
	{
	}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_pump_check_state(select_block_pump_check_state&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _stateHandler(std::move(s._stateHandler)),
		_readySign(s._readySign), _connected(s._connected), _disconnected(s._disconnected), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_has);
	}
#endif
private:
	bool ready()
	{
		assert(!_has);
		assert(!_msgHandle.check_closed());
		_readySign = 1;
		if (!_connected)
		{
			if (_msgHandle.is_connected())
			{
				return true;
			}
			_msgHandle._handle->_waitConnect = true;
			return false;
		}
		if (_msgHandle._handle->read_msg(_has) || is_losted())
		{
			return true;
		}
		_msgHandle._handle->_checkDis = true;
		return _msgHandle._handle->isDisconnected();
	}

	void cancel()
	{
		assert(!_msgHandle.check_closed());
		_msgHandle._handle->stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle.get()->_checkLost);
		return _msgHandle.get()->_losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		assert(!_msgHandle.check_closed());
		if (0 == _readySign)
		{
			if (!_connected && _msgHandle.is_connected())
			{
				_connected = true;
				_disconnected = false;
				isRun = true;
				return _stateHandler(pump_check_state::pump_connected);
			}
			else if (_has)
			{
				isRun = true;
				_lostNtfed = false;
				_has = false;
				return _handler();
			}
			else if (_connected && !_disconnected && !_msgHandle.is_connected())
			{
				_disconnected = true;
				_connected = false;
				isRun = true;
				return _stateHandler(pump_check_state::pump_disconnted);
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _stateHandler(pump_check_state::pump_ntf_losted);
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		assert(!_msgHandle.check_closed());
		return (size_t)_msgHandle._handle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle.get()->_hostActor);
	}
private:
	pump_handle _msgHandle;
	std::function<bool()> _handler;
	std::function<bool(pump_check_state)> _stateHandler;
	int _readySign;
	bool _has;
	bool _connected;
	bool _disconnected;
	bool _lostNtfed;
};
//////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_CHECK_LOST

/*!
@brief msg_handle消息互斥执行块，带通知句柄丢失处理
*/
template <typename... ARGS>
class select_block_msg_check_lost : public MutexBlock_
{
	typedef msg_handle<ARGS...> MsgHandle;
	typedef DstReceiverBuff_<ARGS...> dst_receiver;

	friend my_actor;
public:
	template <typename Handler, typename LostHandler>
	select_block_msg_check_lost(MsgHandle& msgHandle, Handler&& handler, LostHandler&& lostHandler)
		:_msgHandle(msgHandle), _readySign(0), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_lostHandler, std::forward<LostHandler>(lostHandler), msgHandle._hostActor)
	{
		_msgHandle.check_lost(true);
	}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_msg_check_lost(select_block_msg_check_lost&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _lostHandler(std::move(s._lostHandler)),
		_readySign(s._readySign), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_msgBuff.has());
	}
#endif
private:
	bool ready()
	{
		assert(!_msgBuff.has());
		_readySign = 1;
		return _msgHandle.read_msg(_msgBuff) || is_losted();
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle._checkLost);
		return _msgHandle._losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		if (0 == _readySign)
		{
			if (_msgBuff.has())
			{
				isRun = true;
				_lostNtfed = false;
				BREAK_OF_SCOPE_EXEC(_msgBuff.clear());
				return tuple_invoke<bool>(_handler, std::move(_msgBuff._dstBuff.get()));
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _lostHandler();
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool(ARGS...)> _handler;
	std::function<bool()> _lostHandler;
	dst_receiver _msgBuff;
	int _readySign;
	bool _lostNtfed;
};

/*!
@brief trig_handle消息互斥执行块，带通知句柄丢失处理
*/
template <typename... ARGS>
class select_block_trig_check_lost : public MutexBlock_
{
	typedef trig_handle<ARGS...> MsgHandle;
	typedef DstReceiverBuff_<ARGS...> dst_receiver;

	friend my_actor;
public:
	template <typename Handler, typename LostHandler>
	select_block_trig_check_lost(MsgHandle& msgHandle, Handler&& handler, LostHandler&& lostHandler)
		:_msgHandle(msgHandle), _readySign(0), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_lostHandler, std::forward<LostHandler>(lostHandler), msgHandle._hostActor)
	{
		_msgHandle.check_lost(true);
	}

#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_trig_check_lost(select_block_trig_check_lost&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _lostHandler(std::move(s._lostHandler)),
		_readySign(s._readySign), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_msgBuff.has());
	}
#endif
private:
	bool ready()
	{
		_readySign = 1;
		assert(!_msgBuff.has());
		return _msgHandle.read_msg(_msgBuff) || is_losted();
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle._checkLost);
		return _msgHandle._losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		if (0 == _readySign)
		{
			if (_msgBuff.has())
			{
				isRun = true;
				_lostNtfed = false;
				BREAK_OF_SCOPE_EXEC(_msgBuff.clear());
				return tuple_invoke<bool>(_handler, std::move(_msgBuff._dstBuff.get()));
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _lostHandler();
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool(ARGS...)> _handler;
	std::function<bool()> _lostHandler;
	dst_receiver _msgBuff;
	int _readySign;
	bool _lostNtfed;
};

/*!
@brief msg_pump消息互斥执行块，带通知句柄丢失处理
*/
template <typename... ARGS>
class select_block_pump_check_lost : public MutexBlock_
{
	typedef msg_pump_handle<ARGS...> pump_handle;
	typedef DstReceiverBuff_<ARGS...> dst_receiver;

	friend my_actor;
public:
	template <typename Handler, typename LostHandler>
	select_block_pump_check_lost(my_actor* host, Handler&& handler, LostHandler&& lostHandler)
		:select_block_pump_check_lost(0, host, std::forward<Handler>(handler), std::forward<LostHandler>(lostHandler)) {}

	template <typename Handler, typename LostHandler>
	select_block_pump_check_lost(const int id, my_actor* host, Handler&& handler, LostHandler&& lostHandler)
		: select_block_pump_check_lost(ActorFunc_::connect_msg_pump<ARGS...>(id, host, true), std::forward<Handler>(handler), std::forward<LostHandler>(lostHandler)) {}

	template <typename Handler, typename LostHandler>
	select_block_pump_check_lost(const pump_handle& pump, Handler&& handler, LostHandler&& lostHandler)
		:_msgHandle(pump), _readySign(0), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), pump.get()->_hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_lostHandler, std::forward<LostHandler>(lostHandler), pump.get()->_hostActor)
	{
		_msgHandle.check_lost(true);
	}
	
#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_pump_check_lost(select_block_pump_check_lost&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _lostHandler(std::move(s._lostHandler)),
		_readySign(s._readySign), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_msgBuff.has());
	}
#endif
private:
	bool ready()
	{
		assert(!_msgBuff.has());
		assert(!_msgHandle.check_closed());
		_readySign = 1;
		return _msgHandle._handle->read_msg(_msgBuff) || is_losted();
	}

	void cancel()
	{
		assert(!_msgHandle.check_closed());
		_msgHandle._handle->stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle.get()->_checkLost);
		return _msgHandle.get()->_losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		assert(!_msgHandle.check_closed());
		if (0 == _readySign)
		{
			if (_msgBuff.has())
			{
				isRun = true;
				_lostNtfed = false;
				BREAK_OF_SCOPE_EXEC(_msgBuff.clear());
				return tuple_invoke<bool>(_handler, std::move(_msgBuff._dstBuff.get()));
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _lostHandler();
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		assert(!_msgHandle.check_closed());
		return (size_t)_msgHandle._handle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle.get()->_hostActor);
	}
private:
	pump_handle _msgHandle;
	std::function<bool(ARGS...)> _handler;
	std::function<bool()> _lostHandler;
	dst_receiver _msgBuff;
	int _readySign;
	bool _lostNtfed;
};

template <>
class select_block_msg_check_lost<> : public MutexBlock_
{
	typedef msg_handle<> MsgHandle;

	friend my_actor;
public:
	template <typename Handler, typename LostHandler>
	select_block_msg_check_lost(MsgHandle& msgHandle, Handler&& handler, LostHandler&& lostHandler)
		:_msgHandle(msgHandle), _readySign(0), _has(false), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_lostHandler, std::forward<LostHandler>(lostHandler), msgHandle._hostActor)
	{
		_msgHandle.check_lost(true);
	}
	
#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_msg_check_lost(select_block_msg_check_lost&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _lostHandler(std::move(s._lostHandler)),
		_readySign(s._readySign), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_has);
	}
#endif
private:
	bool ready()
	{
		assert(!_has);
		_readySign = 1;
		return _msgHandle.read_msg(_has) || is_losted();
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle._checkLost);
		return _msgHandle._losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		if (0 == _readySign)
		{
			if (_has)
			{
				_has = false;
				isRun = true;
				_lostNtfed = false;
				return _handler();
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _lostHandler();
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool()> _handler;
	std::function<bool()> _lostHandler;
	int _readySign;
	bool _has;
	bool _lostNtfed;
};

template <>
class select_block_trig_check_lost<> : public MutexBlock_
{
	typedef trig_handle<> MsgHandle;

	friend my_actor;
public:
	template <typename Handler, typename LostHandler>
	select_block_trig_check_lost(MsgHandle& msgHandle, Handler&& handler, LostHandler&& lostHandler)
		:_msgHandle(msgHandle), _readySign(0), _has(false), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), msgHandle._hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_lostHandler, std::forward<LostHandler>(lostHandler), msgHandle._hostActor)
	{
		_msgHandle.check_lost(true);
	}
	
#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_trig_check_lost(select_block_trig_check_lost&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _lostHandler(std::move(s._lostHandler)),
		_readySign(s._readySign), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_has);
	}
#endif
private:
	bool ready()
	{
		_readySign = 1;
		assert(!_has);
		return _msgHandle.read_msg(_has) || is_losted();
	}

	void cancel()
	{
		_msgHandle.stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle._checkLost);
		return _msgHandle._losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		if (0 == _readySign)
		{
			if (_has)
			{
				_has = false;
				isRun = true;
				_lostNtfed = false;
				return _handler();
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _lostHandler();
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		return (size_t)&_msgHandle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle._hostActor);
	}
private:
	MsgHandle& _msgHandle;
	std::function<bool()> _handler;
	std::function<bool()> _lostHandler;
	int _readySign;
	bool _has;
	bool _lostNtfed;
};

template <>
class select_block_pump_check_lost<> : public MutexBlock_
{
	typedef msg_pump_handle<> pump_handle;

	friend my_actor;
public:
	template <typename Handler, typename LostHandler>
	select_block_pump_check_lost(my_actor* host, Handler&& handler, LostHandler&& lostHandler)
		:select_block_pump_check_lost(0, host, std::forward<Handler>(handler), std::forward<LostHandler>(lostHandler)) {}

	template <typename Handler, typename LostHandler>
	select_block_pump_check_lost(const int id, my_actor* host, Handler&& handler, LostHandler&& lostHandler)
		: select_block_pump_check_lost(ActorFunc_::connect_msg_pump(id, host, true), std::forward<Handler>(handler), std::forward<LostHandler>(lostHandler)) {}

	template <typename Handler, typename LostHandler>
	select_block_pump_check_lost(const pump_handle& pump, Handler&& handler, LostHandler&& lostHandler)
		:_msgHandle(pump), _readySign(0), _has(false), _lostNtfed(false),
		__MUTEX_BLOCK_HANDLER_WRAP(_handler, std::forward<Handler>(handler), pump.get()->_hostActor),
		__MUTEX_BLOCK_HANDLER_WRAP(_lostHandler, std::forward<LostHandler>(lostHandler), pump.get()->_hostActor)
	{
		_msgHandle.check_lost(true);
	}
	
#if (defined __GNUG__) || (_MSC_VER >= 1900)
	select_block_pump_check_lost(select_block_pump_check_lost&& s)
		:_msgHandle(s._msgHandle), _handler(std::move(s._handler)), _lostHandler(std::move(s._lostHandler)),
		_readySign(s._readySign), _lostNtfed(s._lostNtfed)
	{
		assert(false);
		assert(!_has);
	}
#endif
private:
	bool ready()
	{
		assert(!_has);
		assert(!_msgHandle.check_closed());
		_readySign = 1;
		return _msgHandle._handle->read_msg(_has) || is_losted();
	}

	void cancel()
	{
		assert(!_msgHandle.check_closed());
		_msgHandle._handle->stop_waiting();
		_readySign--;
	}

	bool is_losted()
	{
		assert(_msgHandle.get()->_checkLost);
		return _msgHandle.get()->_losted && !_lostNtfed;
	}

	void check_lost()
	{
	}

	bool go_run(bool& isRun)
	{
		assert(!_msgHandle.check_closed());
		if (0 == _readySign)
		{
			if (_has)
			{
				_has = false;
				_lostNtfed = false;
				isRun = true;
				return _handler();
			}
			else if (is_losted())
			{
				isRun = true;
				_lostNtfed = true;
				return _lostHandler();
			}
		}
		isRun = false;
		return false;
	}

	size_t snap_id()
	{
		assert(!_msgHandle.check_closed());
		return (size_t)_msgHandle._handle;
	}

	long long host_id()
	{
		return MutexBlock_::actor_id(_msgHandle.get()->_hostActor);
	}
private:
	pump_handle _msgHandle;
	std::function<bool()> _handler;
	std::function<bool()> _lostHandler;
	int _readySign;
	bool _has;
	bool _lostNtfed;
};
#endif
//////////////////////////////////////////////////////////////////////////

template <typename Handler, typename... ARGS>
select_block_msg<ARGS...> make_select_block_msg(msg_handle<ARGS...>& msgHandle, Handler&& handler)
{
	return select_block_msg<ARGS...>(msgHandle, std::forward<Handler>(handler));
}

template <typename Handler, typename... ARGS>
select_block_trig<ARGS...> make_select_block_trig(trig_handle<ARGS...>& msgHandle, Handler&& handler)
{
	return select_block_trig<ARGS...>(msgHandle, std::forward<Handler>(handler));
}

template <typename Handler, typename... ARGS>
select_block_pump<ARGS...> make_select_block_pump(const msg_pump_handle<ARGS...>& msgHandle, Handler&& handler)
{
	return select_block_pump<ARGS...>(msgHandle, std::forward<Handler>(handler));
}

template <typename Handler, typename StateHandler, typename... ARGS>
select_block_pump_check_state<ARGS...> make_select_block_pump_check_state(const msg_pump_handle<ARGS...>& msgHandle, Handler&& handler, StateHandler&& stateHandler)
{
	return select_block_pump_check_state<ARGS...>(msgHandle, std::forward<Handler>(handler), std::forward<StateHandler>(stateHandler));
}

#ifdef ENABLE_CHECK_LOST

template <typename Handler, typename LostHandler, typename... ARGS>
select_block_msg_check_lost<ARGS...> make_select_block_msg_check_lost(msg_handle<ARGS...>& msgHandle, Handler&& handler, LostHandler&& lostHandler)
{
	return select_block_msg_check_lost<ARGS...>(msgHandle, std::forward<Handler>(handler), std::forward<LostHandler>(lostHandler));
}

template <typename Handler, typename LostHandler, typename... ARGS>
select_block_trig_check_lost<ARGS...> make_select_block_trig_check_lost(trig_handle<ARGS...>& msgHandle, Handler&& handler, LostHandler&& lostHandler)
{
	return select_block_trig_check_lost<ARGS...>(msgHandle, std::forward<Handler>(handler), std::forward<LostHandler>(lostHandler));
}

template <typename Handler, typename LostHandler, typename... ARGS>
select_block_pump_check_lost<ARGS...> make_select_block_pump_check_lost(const msg_pump_handle<ARGS...>& msgHandle, Handler&& handler, LostHandler&& lostHandler)
{
	return select_block_pump_check_lost<ARGS...>(msgHandle, std::forward<Handler>(handler), std::forward<LostHandler>(lostHandler));
}

#endif
//////////////////////////////////////////////////////////////////////////

class TrigOnceBase_
{
protected:
	TrigOnceBase_();
	TrigOnceBase_(const TrigOnceBase_& s);
	TrigOnceBase_(TrigOnceBase_&& s);
protected:
	template <typename DST, typename... Args>
	void _trig_handler(bool* sign, DST& dstRec, Args&&... args) const
	{
		assert(!_pIsTrig->exchange(true));
		assert(_hostActor);
		ActorFunc_::_trig_handler(_hostActor.get(), sign, dstRec, std::tuple<RM_CREF(Args)...>(std::forward<Args>(args)...));
		reset();
	}

	template <typename DST, typename... Args>
	void _trig_handler2(shared_bool& closed, bool* sign, DST& dstRec, Args&&... args) const
	{
		assert(!_pIsTrig->exchange(true));
		assert(_hostActor);
		ActorFunc_::_trig_handler2(_hostActor.get(), closed, sign, dstRec, std::tuple<RM_CREF(Args)...>(std::forward<Args>(args)...));
		reset();
	}

	template <typename DST, typename SRC>
	void _trig_handler3(bool* sign, DST& dstRec, SRC&& args) const
	{
		assert(!_pIsTrig->exchange(true));
		assert(_hostActor);
		ActorFunc_::_trig_handler(_hostActor.get(), sign, dstRec, std::forward<SRC>(args));
		reset();
	}

	template <typename DST, typename SRC>
	void _trig_handler4(shared_bool& closed, bool* sign, DST& dstRec, SRC&& args) const
	{
		assert(!_pIsTrig->exchange(true));
		assert(_hostActor);
		ActorFunc_::_trig_handler2(_hostActor.get(), closed, sign, dstRec, std::forward<SRC>(args));
		reset();
	}

	void tick_handler(bool* sign) const;
	void tick_handler(shared_bool& closed, bool* sign) const;
	virtual void reset() const = 0;
protected:
	void operator =(const TrigOnceBase_& s);
	void operator =(TrigOnceBase_&& s);
protected:
	mutable actor_handle _hostActor;
	DEBUG_OPERATION(std::shared_ptr<std::atomic<bool> > _pIsTrig);
};


template <typename... ARGS>
class trig_once_notifer : public TrigOnceBase_
{
	typedef std::tuple<ARGS&...> dst_receiver;

	friend my_actor;
public:
	trig_once_notifer() :_dstRec(0) {}
	~trig_once_notifer() {}
private:
	trig_once_notifer(const actor_handle& hostActor, dst_receiver* dstRec, bool* sign)
		:_dstRec(dstRec), _sign(sign)
	{
		_hostActor = hostActor;
	}

	trig_once_notifer(actor_handle&& hostActor, dst_receiver* dstRec, bool* sign)
		:_dstRec(dstRec), _sign(sign)
	{
		_hostActor = std::move(hostActor);
	}
public:
	trig_once_notifer(trig_once_notifer&& s)
		:TrigOnceBase_(std::move(s)), _dstRec(s._dstRec), _sign(s._sign)
	{
		s._dstRec = NULL;
		s._sign = NULL;
	}

	trig_once_notifer(const trig_once_notifer& s)
		:TrigOnceBase_(s), _dstRec(s._dstRec), _sign(s._sign) {}

	void operator =(trig_once_notifer&& s)
	{
		TrigOnceBase_::operator =(std::move(s));
		_dstRec = s._dstRec;
		_sign = s._sign;
		s._dstRec = NULL;
		s._sign = NULL;
	}

	void operator =(const trig_once_notifer& s)
	{
		TrigOnceBase_::operator =(s);
		_dstRec = s._dstRec;
		_sign = s._sign;
	}
public:
	template <typename...  Args>
	void operator()(Args&&... args) const
	{
		static_assert(sizeof...(ARGS) == sizeof...(Args), "");
		_trig_handler(_sign, *_dstRec, std::forward<Args>(args)...);
	}

	void operator()() const
	{
		static_assert(sizeof...(ARGS) == 0, "");
		tick_handler(_sign);
	}

	std::function<void(ARGS...)> case_func() const
	{
		return std::function<void(ARGS...)>(*this);
	}
private:
	void reset() const
	{
		_hostActor.reset();
	}
private:
	bool* _sign;
	dst_receiver* _dstRec;
};
//////////////////////////////////////////////////////////////////////////

template <typename... TYPES>
class callback_handler {};

template <typename... TYPES>
class same_callback_handler {};

template <typename... TYPES>
class asio_callback_handler {};

template <typename... TYPES>
class asio_same_callback_handler {};

template <typename... TYPES>
class sync_cb_handler {};

/*!
@brief 异步回调器(可以多次触发，但只有第一次有效)，作为回调函数参数传入，回调后，自动返回到下一行语句继续执行
*/
template <typename... ARGS, typename... OUTS>
class callback_handler<types_pck<ARGS...>, types_pck<OUTS...>> : public TrigOnceBase_
{
	typedef TrigOnceBase_ Parent;
	typedef std::tuple<OUTS&...> dst_receiver;

	friend my_actor;
public:
	template <typename... Outs>
	callback_handler(my_actor* host, bool hasTm, Outs&... outs)
		:_closed(shared_bool::new_(false)), _selfEarly(host), _bsign(false), _hasTm(hasTm), _sign(&_bsign), _dstRef(outs...)
	{
		Parent::_hostActor = ActorFunc_::shared_from_this(host);
	}

	~callback_handler() __disable_noexcept
	{
		if (_selfEarly)
		{
			if (!ActorFunc_::is_quited(_selfEarly))
			{
				if (!_bsign)
				{
					_bsign = true;
					ActorFunc_::push_yield(_selfEarly);
				}
				_closed = true;
				if (_hasTm)
				{
					ActorFunc_::cancel_timer(_selfEarly);
				}
			}
			Parent::_hostActor.reset();
		}
	}

	callback_handler(const callback_handler& s)
		:TrigOnceBase_(s), _closed(s._closed), _selfEarly(NULL), _bsign(false), _hasTm(s._hasTm), _sign(s._sign), _dstRef(s._dstRef) {}

	callback_handler(callback_handler&& s)
		:TrigOnceBase_(std::move(s)), _closed(s._selfEarly ? s._closed : std::move(s._closed)),
		_selfEarly(NULL), _bsign(false), _hasTm(s._hasTm), _sign(s._sign), _dstRef(s._dstRef)
	{
		s._sign = NULL;
	}
public:
	template <typename... Args>
	void operator()(Args&&... args)
	{
		static_assert(sizeof...(ARGS) == sizeof...(Args), "");
		Parent::_trig_handler2(_closed, _sign, _dstRef, try_ref_move<ARGS>::move(std::forward<Args>(args))...);
	}

	void operator()()
	{
		static_assert(sizeof...(ARGS) == 0, "");
		Parent::tick_handler(_closed, _sign);
	}
private:
	void reset() const
	{
		if (!_selfEarly)
		{
			Parent::_hostActor.reset();
		}
	}

	void operator =(const callback_handler&) = delete;
private:
	shared_bool _closed;
	dst_receiver _dstRef;
	my_actor* const _selfEarly;
	bool* _sign;
	bool _bsign;
	bool _hasTm;
};
/*!
@brief 异步回调器(可以少传参数，可以多次触发，但只有第一次有效)，作为回调函数参数传入，回调后，自动返回到下一行语句继续执行
*/
template <typename... ARGS, typename... OUTS>
class same_callback_handler<types_pck<ARGS...>, types_pck<OUTS...>> : public TrigOnceBase_
{
	typedef TrigOnceBase_ Parent;
	typedef std::tuple<OUTS&...> dst_receiver;

	friend my_actor;
public:
	template <typename... Outs>
	same_callback_handler(my_actor* host, bool hasTm, Outs&... outs)
		:_closed(shared_bool::new_(false)), _selfEarly(host), _bsign(false), _hasTm(hasTm), _sign(&_bsign), _dstRef(outs...)
	{
		Parent::_hostActor = ActorFunc_::shared_from_this(host);
	}

	~same_callback_handler() __disable_noexcept
	{
		if (_selfEarly)
		{
			if (!ActorFunc_::is_quited(_selfEarly))
			{
				if (!_bsign)
				{
					_bsign = true;
					ActorFunc_::push_yield(_selfEarly);
				}
				_closed = true;
				if (_hasTm)
				{
					ActorFunc_::cancel_timer(_selfEarly);
				}
			}
			Parent::_hostActor.reset();
		}
	}

	same_callback_handler(const same_callback_handler& s)
		:TrigOnceBase_(s), _closed(s._closed), _selfEarly(NULL), _bsign(false), _hasTm(s._hasTm), _sign(s._sign), _dstRef(s._dstRef) {}

	same_callback_handler(same_callback_handler&& s)
		:TrigOnceBase_(std::move(s)), _closed(s._selfEarly ? s._closed : std::move(s._closed)),
		_selfEarly(NULL), _bsign(false), _hasTm(s._hasTm), _sign(s._sign), _dstRef(s._dstRef)
	{
		s._sign = NULL;
	}
public:
	template <typename... Args>
	void operator()(Args&&... args)
	{
		static_assert(sizeof...(ARGS) >= sizeof...(Args), "");
		typedef typename types_to_tuple<typename prefix_types<sizeof...(Args), TYPE_PIPE(ARGS)...>::types>::tuple_type tuple_type;
		Parent::_trig_handler4(_closed, _sign, _dstRef, tuple_type(std::forward<Args>(args)...));
	}

	void operator()()
	{
		static_assert(sizeof...(ARGS) >= 0, "");
		Parent::tick_handler(_closed, _sign);
	}
private:
	void reset() const
	{
		if (!_selfEarly)
		{
			Parent::_hostActor.reset();
		}
	}

	void operator =(const same_callback_handler&) = delete;
private:
	shared_bool _closed;
	dst_receiver _dstRef;
	my_actor* const _selfEarly;
	bool* _sign;
	bool _bsign;
	bool _hasTm;
};

/*!
@brief ASIO异步回调器(只能触发一次)，作为回调函数参数传入，回调后，自动返回到下一行语句继续执行
*/
template <typename... ARGS, typename... OUTS>
class asio_callback_handler<types_pck<ARGS...>, types_pck<OUTS...>> : public TrigOnceBase_
{
	typedef TrigOnceBase_ Parent;
	typedef std::tuple<OUTS&...> dst_receiver;

	friend my_actor;
public:
	template <typename... Outs>
	asio_callback_handler(my_actor* host, bool hasTm, Outs&... outs)
		:_selfEarly(host), _bsign(false), _hasTm(hasTm), _sign(&_bsign), _dstRef(outs...)
	{
		Parent::_hostActor = ActorFunc_::shared_from_this(host);
	}

	~asio_callback_handler() __disable_noexcept
	{
		if (_selfEarly)
		{
			assert(!ActorFunc_::is_quited(_selfEarly));
			if (!_bsign)
			{
				_bsign = true;
				ActorFunc_::push_yield(_selfEarly);
			}
			if (_hasTm)
			{
				ActorFunc_::cancel_timer(_selfEarly);
			}
			Parent::_hostActor.reset();
		}
	}

	asio_callback_handler(const asio_callback_handler& s)
		:TrigOnceBase_(std::move((asio_callback_handler&)s)), _selfEarly(NULL), _bsign(false), _hasTm(false), _sign(s._sign), _dstRef(s._dstRef)
	{
		((asio_callback_handler&)s)._sign = NULL;
	}

	asio_callback_handler(asio_callback_handler&& s)
		:TrigOnceBase_(std::move(s)), _selfEarly(NULL), _bsign(false), _hasTm(false), _sign(s._sign), _dstRef(s._dstRef)
	{
		s._sign = NULL;
	}
public:
	template <typename... Args>
	void operator()(Args&&... args) const
	{
		static_assert(sizeof...(ARGS) == sizeof...(Args), "");
		Parent::_trig_handler(_sign, _dstRef, try_ref_move<ARGS>::move(std::forward<Args>(args))...);
	}

	void operator()() const
	{
		static_assert(sizeof...(ARGS) == 0, "");
		Parent::tick_handler(_sign);
	}
private:
	void reset() const
	{
		if (!_selfEarly)
		{
			Parent::_hostActor.reset();
		}
	}

	void operator =(const asio_callback_handler&) = delete;
private:
	dst_receiver _dstRef;
	my_actor* const _selfEarly;
	bool* _sign;
	bool _bsign;
	bool _hasTm;
};

/*!
@brief ASIO异步回调器(可以少传参数，只能触发一次)，作为回调函数参数传入，回调后，自动返回到下一行语句继续执行
*/
template <typename... ARGS, typename... OUTS>
class asio_same_callback_handler<types_pck<ARGS...>, types_pck<OUTS...>> : public TrigOnceBase_
{
	typedef TrigOnceBase_ Parent;
	typedef std::tuple<OUTS&...> dst_receiver;

	friend my_actor;
public:
	template <typename... Outs>
	asio_same_callback_handler(my_actor* host, bool hasTm, Outs&... outs)
		:_selfEarly(host), _bsign(false), _hasTm(hasTm), _sign(&_bsign), _dstRef(outs...)
	{
		Parent::_hostActor = ActorFunc_::shared_from_this(host);
	}

	~asio_same_callback_handler() __disable_noexcept
	{
		if (_selfEarly)
		{
			assert(!ActorFunc_::is_quited(_selfEarly));
			if (!_bsign)
			{
				_bsign = true;
				ActorFunc_::push_yield(_selfEarly);
			}
			if (_hasTm)
			{
				ActorFunc_::cancel_timer(_selfEarly);
			}
			Parent::_hostActor.reset();
		}
	}

	asio_same_callback_handler(const asio_same_callback_handler& s)
		:TrigOnceBase_(std::move((asio_same_callback_handler&)s)), _selfEarly(NULL), _bsign(false), _hasTm(false), _sign(s._sign), _dstRef(s._dstRef)
	{
		((asio_same_callback_handler&)s)._sign = NULL;
	}

	asio_same_callback_handler(asio_same_callback_handler&& s)
		:TrigOnceBase_(std::move(s)), _selfEarly(NULL), _bsign(false), _hasTm(false), _sign(s._sign), _dstRef(s._dstRef)
	{
		s._sign = NULL;
	}
public:
	template <typename... Args>
	void operator()(Args&&... args) const
	{
		static_assert(sizeof...(ARGS) >= sizeof...(Args), "");
		typedef typename types_to_tuple<typename prefix_types<sizeof...(Args), TYPE_PIPE(ARGS)...>::types>::tuple_type tuple_type;
		Parent::_trig_handler3(_sign, _dstRef, tuple_type(std::forward<Args>(args)...));
	}

	void operator()() const
	{
		static_assert(sizeof...(ARGS) >= 0, "");
		Parent::tick_handler(_sign);
	}
private:
	void reset() const
	{
		if (!_selfEarly)
		{
			Parent::_hostActor.reset();
		}
	}

	void operator =(const asio_same_callback_handler&) = delete;
private:
	dst_receiver _dstRef;
	my_actor* const _selfEarly;
	bool* _sign;
	bool _bsign;
	bool _hasTm;
};
//////////////////////////////////////////////////////////////////////////

/*!
@brief 异步回调器(Handler将在回调内部直接执行，可能触发线程安全问题)，作为回调函数参数传入，回调后，自动返回到下一行语句继续执行
*/
template <typename Handler, typename R>
class on_callback_handler : public TrigOnceBase_
{
	typedef RM_CREF(Handler) handler_type;
	typedef TrigOnceBase_ Parent;
	friend my_actor;
public:
	on_callback_handler(my_actor* host, Handler& handler)
		:_closed(shared_bool::new_(false)), _handler(std::forward<Handler>(handler)), _selfEarly(host), _bsign(false), _sign(&_bsign)
	{
		Parent::_hostActor = ActorFunc_::shared_from_this(host);
	}

	~on_callback_handler() __disable_noexcept
	{
		if (_selfEarly)
		{
			if (!ActorFunc_::is_quited(_selfEarly))
			{
				if (!_bsign)
				{
					_bsign = true;
					ActorFunc_::push_yield(_selfEarly);
				}
				_closed = true;
			}
			Parent::_hostActor.reset();
		}
	}

	on_callback_handler(const on_callback_handler& s)
		:TrigOnceBase_(s), _closed(s._closed), _handler(s._handler), _selfEarly(NULL), _bsign(false), _sign(s._sign) {}

	on_callback_handler(on_callback_handler&& s)
		:TrigOnceBase_(std::move(s)), _closed(s._selfEarly ? s._closed : std::move(s._closed)),
		_handler(std::move(s._handler)), _selfEarly(NULL), _bsign(false), _sign(s._sign)
	{
		s._sign = NULL;
	}
public:
	template <typename... Args>
	R operator()(Args&&... args)
	{
		stack_obj<R> result;
		stack_agent_result::invoke(result, _handler, std::forward<Args>(args)...);
		Parent::tick_handler(_closed, _sign);
		return stack_obj_move::move(result);
	}
private:
	void reset() const
	{
		if (!_selfEarly)
		{
			Parent::_hostActor.reset();
		}
	}

	void operator =(const on_callback_handler&) = delete;
private:
	shared_bool _closed;
	handler_type _handler;
	my_actor* const _selfEarly;
	bool* _sign;
	bool _bsign;
};

/*!
@brief 同步回调传入返回值
*/
template <typename R>
struct sync_result
{
	sync_result()
	:_res(NULL), _mutex(NULL), _con(NULL), _sign(false) {}

	template <typename... Args>
	void return_(Args&&... args)
	{
		assert(_res);
		_res->create(std::forward<Args>(args)...);
		{
			assert(_mutex && _con);
			std::lock_guard<std::mutex> lg(*_mutex);
			_con->notify_one();
		}
		reset();
	}

	template <typename T>
	void operator =(T&& r)
	{
		assert(_res);
		_res->create(std::forward<T>(r));
		{
			assert(_mutex && _con);
			std::lock_guard<std::mutex> lg(*_mutex);
			_con->notify_one();
		}
		reset();
	}

	void reset()
	{
		_mutex = NULL;
		_con = NULL;
		_res = NULL;
		_sign = false;
	}

	std::mutex* _mutex;
	std::condition_variable* _con;
	stack_obj<R>* _res;
	bool _sign;
};

/*!
@brief 同步回调器，作为回调函数参数传入，回调后，自动返回到下一行语句继续执行
*/
template <typename R, typename... ARGS, typename... OUTS>
class sync_cb_handler<R, types_pck<ARGS...>, types_pck<OUTS...>> : public TrigOnceBase_
{
	typedef TrigOnceBase_ Parent;
	typedef std::tuple<OUTS&...> dst_receiver;

	friend my_actor;
public:
	template <typename... Outs>
	sync_cb_handler(my_actor* host, sync_result<R>& res, Outs&... outs)
		:_selfEarly(host), _result(res), _dstRef(outs...)
	{
		Parent::_hostActor = ActorFunc_::shared_from_this(host);
		_result.reset();
	}

	~sync_cb_handler() __disable_noexcept
	{
		if (_selfEarly)
		{
			if (!ActorFunc_::is_quited(_selfEarly))
			{
				if (!_result._sign)
				{
					_result._sign = true;
					ActorFunc_::push_yield(_selfEarly);
				}
			}
			Parent::_hostActor.reset();
		}
	}

	sync_cb_handler(const sync_cb_handler& s)
		:TrigOnceBase_(s), _selfEarly(NULL), _result(s._result), _dstRef(s._dstRef) {}

	sync_cb_handler(sync_cb_handler&& s)
		:TrigOnceBase_(std::move(s)), _selfEarly(NULL), _result(s._result), _dstRef(s._dstRef) {}
public:
	template <typename... Args>
	R operator()(Args&&... args)
	{
		static_assert(sizeof...(ARGS) == sizeof...(Args), "");
		assert(!ActorFunc_::self_strand(Parent::_hostActor.get())->in_this_ios());
		stack_obj<R> res;
		_result._res = &res;
		{
			std::mutex mutex;
			std::condition_variable con;
			_result._mutex = &mutex;
			_result._con = &con;
			std::unique_lock<std::mutex> ul(mutex);
			Parent::_trig_handler(&_result._sign, _dstRef, try_ref_move<ARGS>::move(std::forward<Args>(args))...);
			con.wait(ul);
		}
		return stack_obj_move::move(res);
	}

	R operator()()
	{
		static_assert(sizeof...(ARGS) == 0, "");
		assert(!ActorFunc_::self_strand(Parent::_hostActor.get())->in_this_ios());
		stack_obj<R> res;
		_result._res = &res;
		{
			std::mutex mutex;
			std::condition_variable con;
			_result._mutex = &mutex;
			_result._con = &con;
			std::unique_lock<std::mutex> ul(mutex);
			Parent::tick_handler(&_result._sign);
			con.wait(ul);
		}
		return stack_obj_move::move(res);
	}
private:
	void reset() const
	{
		if (!_selfEarly)
		{
			Parent::_hostActor.reset();
		}
	}

	void operator =(const sync_cb_handler& s) = delete;
private:
	dst_receiver _dstRef;
	my_actor* const _selfEarly;
	sync_result<R>& _result;
};
//////////////////////////////////////////////////////////////////////////

template <typename R, typename Handler>
class wrapped_sync_handler
{
	typedef RM_CREF(Handler) handler_type;
public:
	wrapped_sync_handler(Handler& handler, sync_result<R>& res)
		:_handler(std::forward<Handler>(handler)), _result(&res)
	{
		DEBUG_OPERATION(_pIsTrig = std::make_shared<std::atomic<bool> >(false));
	}

	wrapped_sync_handler(const wrapped_sync_handler& s)
		:_handler(s._handler), _result(s._result)
	{
		DEBUG_OPERATION(_pIsTrig = s._pIsTrig);
	}

	wrapped_sync_handler(wrapped_sync_handler&& s)
		:_handler(std::move(s._handler)), _result(s._result)
	{
		s._result = NULL;
		DEBUG_OPERATION(_pIsTrig = std::move(s._pIsTrig));
	}

	void operator =(const wrapped_sync_handler& s)
	{
		_handler = s._handler;
		_result = s._result;
		DEBUG_OPERATION(_pIsTrig = s._pIsTrig);
	}

	void operator =(wrapped_sync_handler&& s)
	{
		_handler = std::move(s._handler);
		_result = s._result;
		s._result = NULL;
		DEBUG_OPERATION(_pIsTrig = std::move(s._pIsTrig));
	}

	template <typename... Args>
	R operator()(Args&&... args)
	{
		stack_obj<R> res;
		_result->_res = &res;
		assert(!_pIsTrig->exchange(true));
		{
			std::mutex mutex;
			std::condition_variable con;
			_result->_mutex = &mutex;
			_result->_con = &con;
			std::unique_lock<std::mutex> ul(mutex);
			_handler(std::forward<Args>(args)...);
			con.wait(ul);
		}
		DEBUG_OPERATION(_pIsTrig->exchange(false));
		return stack_obj_move::move(res);
	}
private:
	handler_type _handler;
	sync_result<R>* _result;
	DEBUG_OPERATION(std::shared_ptr<std::atomic<bool> > _pIsTrig);
};

template <typename R, typename Handler>
class wrapped_sync_handler2
{
	typedef RM_CREF(Handler) handler_type;

	struct sync_st
	{
		std::mutex mutex;
		std::condition_variable con;
	};
public:
	wrapped_sync_handler2(Handler& handler, sync_result<R>& res)
		:_handler(std::forward<Handler>(handler)), _result(&res), _syncSt(new sync_st)
	{
		DEBUG_OPERATION(_pIsTrig = std::make_shared<std::atomic<bool> >(false));
	}

	wrapped_sync_handler2(const wrapped_sync_handler2& s)
		:_handler(s._handler), _result(s._result), _syncSt(s._syncSt)
	{
		DEBUG_OPERATION(_pIsTrig = s._pIsTrig);
	}

	wrapped_sync_handler2(wrapped_sync_handler2&& s)
		:_handler(std::move(s._handler)), _result(s._result), _syncSt(std::move(s._syncSt))
	{
		s._result = NULL;
		DEBUG_OPERATION(_pIsTrig = std::move(s._pIsTrig));
	}

	void operator =(const wrapped_sync_handler2& s)
	{
		_handler = s._handler;
		_syncSt = s._syncSt;
		_result = s._result;
		DEBUG_OPERATION(_pIsTrig = s._pIsTrig);
	}

	void operator =(wrapped_sync_handler2&& s)
	{
		_handler = std::move(s._handler);
		_syncSt = std::move(s._syncSt);
		_result = s._result;
		s._result = NULL;
		DEBUG_OPERATION(_pIsTrig = std::move(s._pIsTrig));
	}

	template <typename... Args>
	R operator()(Args&&... args)
	{
		stack_obj<R> res;
		_result->_res = &res;
		assert(!_pIsTrig->exchange(true));
		{
			_result->_mutex = &_syncSt->mutex;
			_result->_con = &_syncSt->con;
			std::unique_lock<std::mutex> ul(_syncSt->mutex);
			_handler(std::forward<Args>(args)...);
			_syncSt->con.wait(ul);
		}
		DEBUG_OPERATION(_pIsTrig->exchange(false));
		return stack_obj_move::move(res);
	}
private:
	handler_type _handler;
	sync_result<R>* _result;
	std::shared_ptr<sync_st> _syncSt;
	DEBUG_OPERATION(std::shared_ptr<std::atomic<bool> > _pIsTrig);
};

/*!
@brief 包装一个handler到与当前ios无关的线程中同步调用
*/
template <typename R, typename Handler>
wrapped_sync_handler<R, Handler> wrap_sync(sync_result<R>& res, Handler&& handler)
{
	return wrapped_sync_handler<R, Handler>(handler, res);
}

template <typename R, typename Handler>
wrapped_sync_handler2<R, Handler> wrap_sync2(sync_result<R>& res, Handler&& handler)
{
	return wrapped_sync_handler2<R, Handler>(handler, res);
}
//////////////////////////////////////////////////////////////////////////

/*!
@brief 子Actor句柄
*/
class child_handle
{
	friend my_actor;
public:
	typedef std::shared_ptr<child_handle> ptr;
public:
	child_handle();
	child_handle(child_handle&& s);
	~child_handle() __disable_noexcept;
	void operator =(child_handle&& s);
	const actor_handle& get_actor() const;
	my_actor* operator ->() const;
	bool empty() const;
	static ptr make_ptr();
private:
	void peel();
	child_handle(actor_handle&& actor);
private:
	actor_handle _actor;
	trig_handle<> _quiteAth;
	std::list<actor_handle>::iterator _actorIt;
	std::list<std::function<void()> >::iterator _athIt;
	bool _started : 1;
	bool _quited : 1;
	NONE_COPY(child_handle);
};
//////////////////////////////////////////////////////////////////////////

struct AutoStackActorFace_ 
{
	virtual size_t key() = 0;
	virtual size_t stack_size() = 0;
	virtual void swap(std::function<void(my_actor*)>& sk) = 0;
};

template <typename Handler>
struct AutoStackActor_ : public AutoStackActorFace_
{
	AutoStackActor_(Handler& h, size_t stackSize, size_t key)
	:_h(h), _stackSize(stackSize), _key(key) {}

	size_t key()
	{
		return _key;
	}

	size_t stack_size()
	{
		return _stackSize;
	}

	void swap(std::function<void(my_actor*)>& sk)
	{
		sk = (Handler)_h;
	}

	size_t _key;
	size_t _stackSize;
	Handler& _h;
	NONE_COPY(AutoStackActor_);
	RVALUE_CONSTRUCT(AutoStackActor_, _key, _stackSize, _h);
};

template <typename Handler>
struct AutoStackMsgAgentActor_
{
	AutoStackMsgAgentActor_(Handler& h, size_t stackSize, size_t key)
	:_h(h), _stackSize(stackSize), _key(key) {}

	size_t _key;
	size_t _stackSize;
	Handler& _h;
	NONE_COPY(AutoStackMsgAgentActor_);
	RVALUE_CONSTRUCT(AutoStackMsgAgentActor_, _key, _stackSize, _h);
};

struct AutoStack_
{
	AutoStack_(size_t stackSize, size_t key, size_t = 0)
	:_stackSize(stackSize), _key(key) {}

	template <typename Handler>
	AutoStackActor_<Handler&&> operator *(Handler&& handler)
	{
		return AutoStackActor_<Handler&&>(handler, _stackSize, _key);
	}

	size_t _stackSize;
	size_t _key;
	NONE_COPY(AutoStack_);
};

struct AutoStackAgent_
{
	AutoStackAgent_(size_t stackSize, size_t key, size_t = 0)
	:_stackSize(stackSize), _key(key) {}

	template <typename Handler>
	AutoStackMsgAgentActor_<Handler&&> operator *(Handler&& handler)
	{
		return AutoStackMsgAgentActor_<Handler&&>(handler, _stackSize, _key);
	}

	size_t _stackSize;
	size_t _key;
	NONE_COPY(AutoStackAgent_);
};

#ifdef DISABLE_AUTO_STACK

#define auto_stack(...)
#define auto_stack_msg_agent(...)
#define auto_stack_
#define auto_stack_msg_agent_

#else

//自动栈空间控制
#define auto_stack(...) AutoStack_(__VA_ARGS__, __COUNTER__)*
#define auto_stack_msg_agent(...) AutoStackAgent_(__VA_ARGS__, __COUNTER__)*
#define auto_stack_ AutoStack_(0, __COUNTER__)*
#define auto_stack_msg_agent_ AutoStackAgent_(0, __COUNTER__)*

#endif

//////////////////////////////////////////////////////////////////////////

/*!
@brief Actor对象
*/
class my_actor : public ActorTimerFace_
{
	struct suspend_resume_option
	{
		template <typename Handler>
		suspend_resume_option(bool isSuspend, Handler&& h)
			:_isSuspend(isSuspend), _h(std::forward<Handler>(h)) {}

		bool _isSuspend;
		std::function<void()> _h;
		NONE_COPY(suspend_resume_option);
		RVALUE_CONSTRUCT2(suspend_resume_option, _isSuspend, _h);
	};

	struct msg_pool_status
	{
#if (_WIN64 || __x86_64__ || _ARM64)
		struct id_key
		{
			id_key(const size_t hash, const int id)
			:_id(((unsigned long long)id << 56) | hash) { assert(0 == ((0xFFLL << 56) & hash)); }

			operator unsigned long long() const
			{
				return _id;
			}

			const unsigned long long _id;
		};
#else
		struct id_key
		{
			id_key(const size_t hash, const int id)
			:_id(((unsigned long long)id << 32) | hash) {}

			operator unsigned long long() const
			{
				return _id;
			}

			const unsigned long long _id;
		};
#endif

		msg_pool_status()
			:_msgTypeMap(*_msgTypeMapAll) {}

		~msg_pool_status() {}

		struct pck_base
		{
			pck_base(my_actor* hostActor)
				:_amutex(hostActor->_strand), _isHead(true), _hostActor(hostActor){}

			virtual ~pck_base() {}

			virtual void close() = 0;
			virtual void clear() = 0;

			bool is_closed()
			{
				return !_hostActor;
			}

			void lock(my_actor* self)
			{
				_amutex.lock(self);
			}

			void unlock(my_actor* self)
			{
				_amutex.unlock(self);
			}

			actor_mutex _amutex;
			my_actor* _hostActor;
			bool _isHead;
		private:
			pck_base();
		};

		template <typename... ARGS>
		struct pck : public pck_base
		{
			typedef MsgPool_<ARGS...> pool_type;
			typedef MsgPump_<ARGS...> pump_type;

			pck(my_actor* hostActor)
				:pck_base(hostActor){}

			void close()
			{
				assert(!_next || !_next->_next);
				if (_msgPool)
				{
					if (_isHead)
					{
						_msgPool->_closed = true;
					}
					else
					{
						_hostActor->send_after_quited(_msgPool->_strand, [&]
						{
							assert(_msgPool->_msgPump == _msgPump);
							_msgPool->disconnect();
						});
					}
				}
				if (_msgPump)
				{
					if (!_isHead && _msgPump->_hasMsg)
					{
						typedef typename pump_type::msg_type msg_type;
						assert(_msgPool);
						stack_obj<msg_type> suck;
						bool losted = false;
						_msgPump->backflow(suck, losted);
						if (suck.has() || losted)
						{
							_hostActor->send_after_quited(_msgPool->_strand, [this, &suck]
							{
								_msgPool->backflow(suck);
							});
						}
					}
					_msgPump->close();
				}
				_msgPump.reset();
				_msgPool.reset();
				_next.reset();
				_hostActor = NULL;
				_isHead = false;
			}

			void clear()
			{
				if (_isHead && _msgPool)
				{
					_msgPool->_closed = true;
				}
				if (_msgPump)
				{
					_msgPump->close();
				}
				_msgPump.reset();
				_msgPool.reset();
				_next.reset();
				_hostActor = NULL;
				_isHead = false;
			}

			std::shared_ptr<pool_type> _msgPool;
			std::shared_ptr<pump_type> _msgPump;
			std::shared_ptr<pck> _next;
			NONE_COPY(pck)
		};

		void clear(my_actor* self)
		{
			for (auto it = _msgTypeMap.begin(); it != _msgTypeMap.end(); ++it) { it->second->_amutex.quited_lock(self); }
			for (auto it = _msgTypeMap.begin(); it != _msgTypeMap.end(); ++it) { it->second->close(); }
			for (auto it = _msgTypeMap.begin(); it != _msgTypeMap.end(); ++it) { it->second->_amutex.quited_unlock(self); }
			_msgTypeMap.clear();
		}

		msg_map_shared_alloc<id_key, std::shared_ptr<pck_base> > _msgTypeMap;
		static msg_map_shared_alloc<id_key, std::shared_ptr<pck_base> >::shared_node_alloc* _msgTypeMapAll;
	};

	template <typename R>
	struct async_invoke_handler
	{
		async_invoke_handler(actor_handle&& host, bool* sign, stack_obj<R>& dst)
			:_sharedThis(std::move(host)), _sign(sign), _dstRec(dst) {}

		template <typename Arg>
		void operator ()(Arg&& arg)
		{
			_dstRec.create(std::forward<Arg>(arg));
			_sharedThis->tick_handler(_sign);
		}

		void operator ()()
		{
			_sharedThis->tick_handler(_sign);
		}

		actor_handle _sharedThis;
		stack_obj<R>& _dstRec;
		bool* _sign;
	private:
		void operator =(const async_invoke_handler&) = delete;
		COPY_CONSTRUCT3(async_invoke_handler, _sharedThis, _dstRec, _sign);
	};

	struct wrap_trig_run_one
	{
		wrap_trig_run_one(actor_handle&& self, bool* sign)
			:_lockSelf(std::move(self)), _sign(sign) {}

		void operator()()
		{
			if (!_lockSelf->_quited)
			{
				run_one(_lockSelf.get(), _sign);
			}
		}

		static void run_one(my_actor* host, bool* sign)
		{
			assert(!host->_quited);
			if (*sign)
			{
				host->pull_yield();
			}
			else
			{
				*sign = true;
			}
		}

		actor_handle _lockSelf;
		bool* _sign;
	private:
		void operator =(const wrap_trig_run_one&) = delete;
		COPY_CONSTRUCT2(wrap_trig_run_one, _lockSelf, _sign);
	};

	struct wrap_check_trig_run_one
	{
		template <typename ActorHandle>
		wrap_check_trig_run_one(shared_bool& closed, ActorHandle&& self, bool* sign)
			:_closed(closed), _lockSelf(std::forward<ActorHandle>(self)), _sign(sign) {}

		void operator()()
		{
			if (!_lockSelf->_quited && !_closed)
			{
				run_one(_lockSelf.get(), _closed, _sign);
			}
		}

		static void run_one(my_actor* host, shared_bool& closed, bool* sign)
		{
			assert(!host->_quited && !closed);
			closed = true;
			if (*sign)
			{
				host->pull_yield();
			}
			else
			{
				*sign = true;
			}
		}

		shared_bool _closed;
		actor_handle _lockSelf;
		bool* _sign;
	private:
		void operator =(const wrap_check_trig_run_one&) = delete;
		COPY_CONSTRUCT3(wrap_check_trig_run_one, _closed, _lockSelf, _sign);
	};

	struct wrap_timer_handler_face
	{
		virtual void invoke() = 0;
		virtual void destroy() = 0;
	};

	template <typename Handler>
	struct wrap_timer_handler : public wrap_timer_handler_face
	{
		typedef RM_CREF(Handler) handler_type;

		wrap_timer_handler(Handler& handler)
			:_handler(std::forward<Handler>(handler)) {}

		void invoke()
		{
			CHECK_EXCEPTION(_handler);
			destroy();
		}

		void destroy()
		{
			this->~wrap_timer_handler();
		}

		handler_type _handler;
		NONE_COPY(wrap_timer_handler);
	};

	template <typename T>
	struct ignore_msg
	{
		template <typename Arg>
		void operator=(Arg&&) {}
	};

	class actor_run;
	friend actor_run;
	friend child_handle;
	friend select_block_quit;
	friend select_block_sign;
	friend MsgPumpBase_;
	friend msg_handle_base;
	friend TrigOnceBase_;
	friend io_engine;
	friend ActorTimer_;
	friend MutexBlock_;
	friend ActorFunc_;
public:
	/*!
	@brief 在{}一定范围内锁定当前Actor不被强制退出，如果锁定期间被挂起，将无法等待其退出
	*/
	class quit_guard
	{
	public:
		quit_guard(my_actor* self);
		~quit_guard() __disable_noexcept;
		void lock();
		void unlock();
	private:
		my_actor* _self;
		bool _locked;
		NONE_COPY(quit_guard)
	};

	/*!
	@brief 在{}一定范围内锁定当前Actor不被挂起
	*/
	class suspend_guard
	{
	public:
		suspend_guard(my_actor* self);
		~suspend_guard() __disable_noexcept;
		void lock();
		void unlock();
	private:
		my_actor* _self;
		bool _locked;
		NONE_COPY(suspend_guard)
	};

	/*!
	@brief Actor被强制退出的异常类型
	*/
	struct force_quit_exception { };

	/*!
	@brief 栈空间耗尽异常
	*/
	struct stack_exhaustion_exception { };

	/*!
	@brief Actor入口函数体
	*/
	typedef std::function<void(my_actor*)> main_func;

	/*!
	@brief actor id
	*/
	typedef long long id;
private:
	my_actor();
	my_actor(const my_actor&);
	my_actor& operator =(const my_actor&);
	~my_actor();
public:
	/*!
	@brief 创建一个Actor
	@param actorStrand Actor所依赖的strand
	@param mainFunc Actor执行入口
	@param stackSize Actor栈大小，默认64k字节，必须是4k的整数倍，最小4k，最大1M
	*/
	static actor_handle create(shared_strand actorStrand, main_func mainFunc, size_t stackSize = DEFAULT_STACKSIZE);
	static actor_handle create(shared_strand actorStrand, AutoStackActorFace_&& wrapActor);

	template <typename SharedStrand, typename MainFunc, typename NotifyFunc>
	static actor_handle create_and_notify(SharedStrand&& actorStrand, MainFunc&& mainFunc, NotifyFunc&& notifyFunc, size_t stackSize = DEFAULT_STACKSIZE)
	{
		actor_handle newActor = create(std::forward<SharedStrand>(actorStrand), std::forward<MainFunc>(mainFunc), stackSize);
		newActor->_quitCallback.push_back(std::forward<NotifyFunc>(notifyFunc));
		return newActor;
	}

	template <typename SharedStrand, typename NotifyFunc>
	static actor_handle create_and_notify(SharedStrand&& actorStrand, AutoStackActorFace_&& wrapActor, NotifyFunc&& notifyFunc)
	{
		actor_handle newActor = create(std::forward<SharedStrand>(actorStrand), std::move(wrapActor));
		newActor->_quitCallback.push_back(std::forward<NotifyFunc>(notifyFunc));
		return newActor;
	}
public:
	/*!
	@brief 创建一个子Actor，父Actor终止时，子Actor也终止（在子Actor都完全退出后，父Actor才结束）
	@param actorStrand 子Actor依赖的strand
	@param mainFunc 子Actor入口函数
	@param stackSize Actor栈大小，4k的整数倍（最大1MB）
	@return 子Actor句柄
	*/
	child_handle create_child(shared_strand actorStrand, main_func mainFunc, size_t stackSize = DEFAULT_STACKSIZE);
	child_handle create_child(main_func mainFunc, size_t stackSize = DEFAULT_STACKSIZE);
	child_handle create_child(shared_strand actorStrand, AutoStackActorFace_&& wrapActor);
	child_handle create_child(AutoStackActorFace_&& wrapActor);

	/*!
	@brief 开始运行子Actor，只能调用一次
	*/
	void child_run(child_handle& actorHandle);

	template <typename... Handles>
	void child_run(child_handle& actorHandle, Handles&... handles)
	{
		static_assert(sizeof...(Handles) >= 1, "");
		child_run(actorHandle);
		child_run(handles...);
	}

	/*!
	@brief 开始运行一组子Actor，只能调用一次
	*/
	void children_run(std::list<child_handle::ptr>& actorHandles);
	void children_run(std::list<child_handle>& actorHandles);

	template <typename Alloc>
	void children_run(std::list<child_handle::ptr, Alloc>& actorHandles)
	{
		assert_enter();
		for (auto& actorHandle : actorHandles)
		{
			child_run(*actorHandle);
		}
	}

	template <typename Alloc>
	void children_run(std::list<child_handle, Alloc>& actorHandles)
	{
		assert_enter();
		for (auto& actorHandle : actorHandles)
		{
			child_run(actorHandle);
		}
	}

	/*!
	@brief 强制终止一个子Actor
	*/
	__yield_interrupt void child_force_quit(child_handle& actorHandle);

	template <typename... Handles>
	__yield_interrupt void child_force_quit(child_handle& actorHandle, Handles&... handles)
	{
		static_assert(sizeof...(Handles) >= 1, "");
		child_force_quit(actorHandle);
		child_force_quit(handles...);
	}

	/*!
	@brief 强制终止一组Actor
	*/
	__yield_interrupt void children_force_quit(std::list<child_handle::ptr>& actorHandles);
	__yield_interrupt void children_force_quit(std::list<child_handle>& actorHandles);

	template <typename Alloc>
	__yield_interrupt void children_force_quit(std::list<child_handle::ptr, Alloc>& actorHandles)
	{
		assert_enter();
		for (auto& actorHandle : actorHandles)
		{
			if (!actorHandle->_quited)
			{
				assert(actorHandle->get_actor());
				assert((*actorHandle)->parent_actor()->self_id() == self_id());
				(*actorHandle)->force_quit();
			}
		}
		for (auto& actorHandle : actorHandles)
		{
			if (!actorHandle->_quited)
			{
				wait_trig(actorHandle->_quiteAth);
				actorHandle->peel();
			}
		}
	}

	template <typename Alloc>
	__yield_interrupt void children_force_quit(std::list<child_handle, Alloc>& actorHandles)
	{
		assert_enter();
		for (auto& actorHandle : actorHandles)
		{
			if (!actorHandle._quited)
			{
				assert(actorHandle.get_actor());
				assert(actorHandle->parent_actor()->self_id() == self_id());
				actorHandle->force_quit();
			}
		}
		for (auto& actorHandle : actorHandles)
		{
			if (!actorHandle._quited)
			{
				wait_trig(actorHandle._quiteAth);
				actorHandle.peel();
			}
		}
	}

	/*!
	@brief 等待一个子Actor完成后返回
	@return 正常退出的返回true，否则false
	*/
	__yield_interrupt void child_wait_quit(child_handle& actorHandle);

	template <typename... Handles>
	__yield_interrupt void child_wait_quit(child_handle& actorHandle, Handles&... handles)
	{
		static_assert(sizeof...(Handles) >= 1, "");
		child_wait_quit(actorHandle);
		child_wait_quit(handles...);
	}

	__yield_interrupt bool timed_child_wait_quit(int ms, child_handle& actorHandle);

	/*!
	@brief 等待一组子Actor完成后返回
	@return 都正常退出的返回true，否则false
	*/
	__yield_interrupt void children_wait_quit(std::list<child_handle::ptr>& actorHandles);
	__yield_interrupt void children_wait_quit(std::list<child_handle>& actorHandles);

	template <typename Alloc>
	__yield_interrupt void children_wait_quit(std::list<child_handle::ptr, Alloc>& actorHandles)
	{
		assert_enter();
		for (auto& actorHandle : actorHandles)
		{
			assert(actorHandle->get_actor()->parent_actor()->self_id() == self_id());
			child_wait_quit(*actorHandle);
		}
	}

	template <typename Alloc>
	__yield_interrupt void children_wait_quit(std::list<child_handle, Alloc>& actorHandles)
	{
		assert_enter();
		for (auto& actorHandle : actorHandles)
		{
			assert(actorHandle.get_actor()->parent_actor()->self_id() == self_id());
			child_wait_quit(actorHandle);
		}
	}

	/*!
	@brief 挂起子Actor
	*/
	__yield_interrupt void child_suspend(child_handle& actorHandle);

	template <typename... Handles>
	__yield_interrupt void child_suspend(child_handle& actorHandle, Handles&... handles)
	{
		static_assert(sizeof...(Handles) >= 1, "");
		lock_quit();
		child_suspend(actorHandle);
		child_suspend(handles...);
		unlock_quit();
	}

	/*!
	@brief 挂起一组子Actor
	*/
	__yield_interrupt void children_suspend(std::list<child_handle::ptr>& actorHandles);
	__yield_interrupt void children_suspend(std::list<child_handle>& actorHandles);

	template <typename Alloc>
	__yield_interrupt void children_suspend(std::list<child_handle::ptr, Alloc>& actorHandles)
	{
		assert_enter();
		lock_quit();
		msg_handle<> amh;
		msg_notifer<> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : actorHandles)
		{
			assert(actorHandle->get_actor());
			assert((*actorHandle)->parent_actor()->self_id() == self_id());
			(*actorHandle)->suspend(wrap_ref_handler(h));
		}
		for (size_t i = actorHandles.size(); i > 0; i--)
		{
			wait_msg(amh);
		}
		close_msg_notifer(amh);
		unlock_quit();
	}

	template <typename Alloc>
	__yield_interrupt void children_suspend(std::list<child_handle, Alloc>& actorHandles)
	{
		assert_enter();
		lock_quit();
		msg_handle<> amh;
		msg_notifer<> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : actorHandles)
		{
			assert(actorHandle.get_actor());
			assert(actorHandle->parent_actor()->self_id() == self_id());
			actorHandle->suspend(wrap_ref_handler(h));
		}
		for (size_t i = actorHandles.size(); i > 0; i--)
		{
			wait_msg(amh);
		}
		close_msg_notifer(amh);
		unlock_quit();
	}

	/*!
	@brief 恢复子Actor
	*/
	__yield_interrupt void child_resume(child_handle& actorHandle);

	template <typename... Handles>
	__yield_interrupt void child_resume(child_handle& actorHandle, Handles&... handles)
	{
		static_assert(sizeof...(Handles) >= 1, "");
		lock_quit();
		child_resume(actorHandle);
		child_resume(handles...);
		unlock_quit();
	}

	/*!
	@brief 恢复一组子Actor
	*/
	__yield_interrupt void children_resume(std::list<child_handle::ptr>& actorHandles);
	__yield_interrupt void children_resume(std::list<child_handle>& actorHandles);

	template <typename Alloc>
	__yield_interrupt void children_resume(std::list<child_handle::ptr, Alloc>& actorHandles)
	{
		assert_enter();
		lock_quit();
		msg_handle<> amh;
		msg_notifer<> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : actorHandles)
		{
			assert(actorHandle->get_actor());
			assert((*actorHandle)->parent_actor()->self_id() == self_id());
			(*actorHandle)->resume(wrap_ref_handler(h));
		}
		for (size_t i = actorHandles.size(); i > 0; i--)
		{
			wait_msg(amh);
		}
		close_msg_notifer(amh);
		unlock_quit();
	}

	template <typename Alloc>
	__yield_interrupt void children_resume(std::list<child_handle, Alloc>& actorHandles)
	{
		assert_enter();
		lock_quit();
		msg_handle<> amh;
		msg_notifer<> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : actorHandles)
		{
			assert(actorHandle.get_actor());
			assert(actorHandle->parent_actor()->self_id() == self_id());
			actorHandle->resume(wrap_ref_handler(h));
		}
		for (size_t i = actorHandles.size(); i > 0; i--)
		{
			wait_msg(amh);
		}
		close_msg_notifer(amh);
		unlock_quit();
	}

	/*!
	@brief 创建另一个Actor，Actor执行完成后返回
	*/
	__yield_interrupt void run_child_complete(shared_strand actorStrand, const main_func& h, size_t stackSize = DEFAULT_STACKSIZE);
	__yield_interrupt void run_child_complete(const main_func& h, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 运行一个generator函数体，完成后返回
	*/
	template <typename Handler>
	__yield_interrupt void run_generator(shared_strand actorStrand, Handler&& h)
	{
		assert_enter();
		lock_quit();
		trig([&](trig_once_notifer<>&& cb)
		{
			generator::create(std::move(actorStrand), wrap_ref_handler(h), std::move(cb))->run();
		});
		unlock_quit();
	}

	template <typename Handler>
	__yield_interrupt void run_generator(Handler&& h)
	{
		run_generator(_strand, std::forward<Handler>(h));
	}

	/*!
	@brief 延时等待
	@param ms/us 等待时间，等于0时暂时放弃Actor执行，直到下次被调度器触发
	*/
	__yield_interrupt void sleep(int ms);
	__yield_interrupt void sleep_guard(int ms);
	__yield_interrupt void usleep(long long us);
	__yield_interrupt void usleep_guard(long long us);

	/*!
	@brief 以绝对时间sleep
	*/
	__yield_interrupt void dead_sleep(long long ms);
	__yield_interrupt void dead_sleep_guard(long long ms);
	__yield_interrupt void dead_usleep(long long us);
	__yield_interrupt void dead_usleep_guard(long long us);

	/*!
	@brief 中断当前时间片，等到下次被调度(因为Actor是非抢占式调度，当有占用时间片较长的逻辑时，适当使用yield分割时间片)
	*/
	__yield_interrupt void yield();

	/*!
	@brief 尝试yield，如果当前yield计数和上次try_yield没变，就yield一次
	*/
	__yield_interrupt void try_yield();

	/*!
	@brief 锁定退出后，中断时间片
	*/
	__yield_interrupt void tick_yield();

	/*!
	@brief 尝试yield_guard，如果当前yield计数和上次try_yield没变，就yield_guard一次
	*/
	__yield_interrupt void try_tick_yield();

	/*!
	@brief 获取父Actor
	*/
	const actor_handle& parent_actor();

	/*!
	@brief 获取子Actor
	*/
	const std::list<actor_handle>& children();
public:
	typedef std::list<std::function<void()> >::iterator quit_iterator;

	/*!
	@brief 注册一个资源释放函数，在强制准备退出Actor时执行
	*/
	quit_iterator regist_quit_executor(std::function<void()> quitHandler);

	/*!
	@brief 注销资源释放函数
	*/
	void cancel_quit_executor(const quit_iterator& qh);
public:
	/*!
	@brief 使用内部定时器延时触发某个函数，在触发完成之前不能多次调用
	@param ms 触发延时(毫秒)
	@param handler 触发函数
	*/
	template <typename Handler>
	void delay_trig(int ms, Handler&& handler)
	{
		assert_enter();
		if (ms >= 0)
		{
			timeout(ms, std::forward<Handler>(handler));
		}
		else
		{
			assert(false);
		}
	}

	/*!
	@brief 使用内部定时器在绝对时间触发某个函数，在触发完成之前不能多次调用
	@param us 触发时间(微秒)
	@param handler 触发函数
	*/
	template <typename Handler>
	void deadline_trig(long long us, Handler&& handler)
	{
		assert_enter();
		deadline(us, std::forward<Handler>(handler));
	}

	/*!
	@brief 取消内部定时器触发
	*/
	void cancel_delay_trig();
public:
	/*!
	@brief 发送一个异步函数到shared_strand中执行（如果是和当前一样的shared_strand直接执行），配合quit_guard使用防止引用失效，完成后返回
	*/
	template <typename H>
	__yield_interrupt auto send(const shared_strand& exeStrand, H&& h)->decltype(h())
	{
		return send<decltype(h())>(exeStrand, std::forward<H>(h));
	}

	template <typename R, typename H>
	__yield_interrupt R send(const shared_strand& exeStrand, H&& h)
	{
		assert_enter();
		quit_guard qg(this);
		if (exeStrand != _strand)
		{
			stack_obj<R> res;
			exeStrand->post(std::bind([&h, &res](actor_handle& shared_this)
			{
				CHECK_EXCEPTION(stack_agent_result::invoke, res, h);
				my_actor* const self = shared_this.get();
				self->_strand->post(std::bind([](actor_handle& shared_this)
				{
					shared_this->pull_yield();
				}, std::move(shared_this)));
			}, shared_from_this()));
			push_yield();
			return stack_obj_move::move(res);
		}
		return h();
	}

	/*!
	@brief 往当前"系统线程"堆栈中抛出一个任务，完成后返回，（用于消耗堆栈高的函数）
	*/
	template <typename H>
	__yield_interrupt auto run_in_thread_stack(H&& h)->decltype(h())
	{
		return run_in_thread_stack<decltype(h())>(std::forward<H>(h));
	}

	template <typename R, typename H>
	__yield_interrupt R run_in_thread_stack(H&& h)
	{
		assert_enter();
		quit_guard qg(this);
		stack_obj<R> res;
		_strand->next_tick(std::bind([&h, &res](actor_handle& shared_this)
		{
			CHECK_EXCEPTION(stack_agent_result::invoke, res, h);
			shared_this->pull_yield();
		}, shared_from_this()));
		push_yield();
		return stack_obj_move::move(res);
	}

	/*!
	@brief 切换到一个大空间栈内运行一个任务，里面不能进行切换操作
	*/
	template <typename H>
	auto run_in_safe_stack(H&& h)->decltype(h())
	{
		return run_in_safe_stack<decltype(h())>(std::forward<H>(h));
	}

	template <typename R, typename H>
	R run_in_safe_stack(H&& h)
	{
		assert_enter();
		stack_obj<R> res;
		auto th = [&]{CHECK_EXCEPTION(stack_agent_result::invoke, res, h); };
		self_io_engine().switchInvoke(wrap_local_handler(th));
		return stack_obj_move::move(res);
	}

	/*!
	@brief 强制将一个函数发送到一个shared_strand中执行（比如某个API会进行很多层次的堆栈调用，而当前Actor堆栈不够，可以用此切换到线程堆栈中直接执行），
	配合quit_guard使用防止引用失效，完成后返回
	*/
	template <typename H>
	__yield_interrupt auto async_send(const shared_strand& exeStrand, H&& h)->decltype(h())
	{
		return async_send<decltype(h())>(exeStrand, std::forward<H>(h));
	}

	template <typename R, typename H>
	__yield_interrupt R async_send(const shared_strand& exeStrand, H&& h)
	{
		assert_enter();
		quit_guard qg(this);
		bool sign = false;
		stack_obj<R> res;
		exeStrand->async_invoke(std::forward<H>(h), async_invoke_handler<R>(shared_from_this(), &sign, res));
		assert(!sign);
		sign = true;
		push_yield();
		return stack_obj_move::move(res);
	}

	template <typename H>
	__yield_interrupt auto async_send_self(H&& h)->decltype(h())
	{
		return async_send_self<decltype(h())>(std::forward<H>(h));
	}

	template <typename R, typename H>
	__yield_interrupt R async_send_self(H&& h)
	{
		return async_send<R>(_strand, std::forward<H>(h));
	}

	template <typename H>
	__yield_interrupt bool timed_send(int ms, const shared_strand& exeStrand, H&& h)
	{
		assert_enter();
		quit_guard qg(this);
		if (exeStrand != _strand)
		{
			bool running = false;
			trig_handle<> ath;
			trig_notifer<> ntf = make_trig_notifer_to_self(ath);
			std::shared_ptr<std::mutex> mutex = std::make_shared<std::mutex>();
			exeStrand->post(std::bind([&h, &running, &ntf](std::shared_ptr<std::mutex>& mutex, shared_bool& deadSign)
			{
				bool run = false;
				mutex->lock();
				if (!deadSign)
				{
					running = true;
					run = true;
				}
				mutex->unlock();
				if (run)
				{
					CHECK_EXCEPTION(h);
					ntf();
				}
			}, mutex, ath.dead_sign()));
			if (!timed_wait_trig(ms, ath))
			{
				mutex->lock();
				if (!running)
				{
					close_trig_notifer(ath);
					mutex->unlock();
					return false;
				}
				mutex->unlock();
				wait_trig(ath);
			}
			return true;
		}
		else
		{
			CHECK_EXCEPTION(h);
			return true;
		}
	}
private:
	template <typename H>
	__yield_interrupt void send_after_quited(const shared_strand& exeStrand, H&& h)
	{
		if (exeStrand != _strand)
		{
			exeStrand->post(std::bind([&h](actor_handle& shared_this)
			{
				CHECK_EXCEPTION(h);
				my_actor* const self = shared_this.get();
				self->_strand->post(std::bind([](actor_handle& shared_this)
				{
					shared_this->pull_yield_after_quited();
				}, std::move(shared_this)));
			}, shared_from_this()));
			push_yield_after_quited();
		}
		else
		{
			CHECK_EXCEPTION(h);
		}
	}

	template <typename H>
	__yield_interrupt void run_in_safe_stack_after_quited(H&& h)
	{
		self_io_engine().switchInvoke(wrap_local_handler(h));
	}
public:
	/*!
	@brief 调用一个异步函数，异步回调完成后返回
	*/
	template <typename... Outs, typename Func>
	__yield_interrupt void trig(Outs&... dargs, Func&& h)
	{
		assert_enter();
		bool sign = false;
		std::tuple<Outs&...> res(dargs...);
		CHECK_EXCEPTION(h, trig_once_notifer<Outs...>(shared_from_this(), &res, &sign));
		if (!sign)
		{
			sign = true;
			push_yield();
		}
	}

	template <typename Func>
	__yield_interrupt void trig(Func&& h)
	{
		assert_enter();
		bool sign = false;
		CHECK_EXCEPTION(h, trig_once_notifer<>(shared_from_this(), NULL, &sign));
		if (!sign)
		{
			sign = true;
			push_yield();
		}
	}

	template <typename R, typename Func>
	__yield_interrupt R trig(Func&& h)
	{
		R res;
		trig<R>(res, h);
		return res;
	}

	/*!
	@brief 调用一个异步函数，异步回调完成后返回，之间锁定强制退出
	*/
	template <typename... Outs, typename Func>
	__yield_interrupt void trig_guard(Outs&... dargs, Func&& h)
	{
		assert_enter();
		lock_quit();
		bool sign = false;
		std::tuple<Outs&...> res(dargs...);
		_strand->next_tick(std::bind([&h, &res, &sign](actor_handle& shared_this)
		{
			h(trig_once_notifer<Outs...>(std::move(shared_this), &res, &sign));
		}, shared_from_this()));
		if (!sign)
		{
			sign = true;
			push_yield();
		}
		unlock_quit();
	}

	template <typename Func>
	__yield_interrupt void trig_guard(Func&& h)
	{
		assert_enter();
		lock_quit();
		bool sign = false;
		_strand->next_tick(std::bind([&h, &sign](actor_handle& shared_this)
		{
			h(trig_once_notifer<>(std::move(shared_this), NULL, &sign));
		}, shared_from_this()));
		if (!sign)
		{
			sign = true;
			push_yield();
		}
		unlock_quit();
	}

	template <typename R, typename Func>
	__yield_interrupt R trig_guard(Func&& h)
	{
		R res;
		trig_guard<R>(res, h);
		return res;
	}
private:
	void tick_handler(bool* sign);
	void tick_handler(shared_bool& closed, bool* sign);

	template <typename DST, typename SRC>
	void _trig_handler(bool* sign, DST& dstRec, SRC&& args)
	{
		assert(!_quited);
		same_copy_tuple_to_tuple(dstRec, std::forward<SRC>(args));
		if (_strand->running_in_this_thread())
		{
			wrap_trig_run_one::run_one(this, sign);
		}
		else
		{
			_strand->post(std::bind([sign](actor_handle& shared_this)
			{
				wrap_trig_run_one::run_one(shared_this.get(), sign);
			}, shared_from_this()));
		}
	}

	template <typename DST, typename SRC>
	void _trig_handler2(shared_bool& closed, bool* sign, DST& dstRec, SRC&& args)
	{
		if (_strand->running_in_this_thread())
		{
			if (!_quited && !closed)
			{
				same_copy_tuple_to_tuple(dstRec, std::forward<SRC>(args));
				wrap_check_trig_run_one::run_one(this, closed, sign);
			}
		}
		else
		{
			_strand->post(std::bind([sign](actor_handle& shared_this, shared_bool& closed, DST& dstRec, SRC& args)
			{
				if (!shared_this->_quited && !closed)
				{
					same_copy_tuple_to_tuple(dstRec, std::move(args));
					wrap_check_trig_run_one::run_one(shared_this.get(), closed, sign);
				}
			}, shared_from_this(), closed, dstRec, std::forward<SRC>(args)));
		}
	}
private:
	bool _timed_wait_msg(int ms, const wrap_local_handler_face<void()>& th, ActorMsgHandlePush_<>& amh);

	bool _timed_wait_trig_sign(int ms, int id, const wrap_local_handler_face<void()>& th);

	template <typename DST, typename... Args>
	bool _timed_wait_msg(int ms, const wrap_local_handler_face<void()>& th, ActorMsgHandlePush_<Args...>& amh, DST& dstRec)
	{
		assert(amh._hostActor && amh._hostActor->self_id() == self_id());
		if (!amh.read_msg(dstRec))
		{
			BREAK_OF_SCOPE_EXEC(amh.stop_waiting());
#ifdef ENABLE_CHECK_LOST
			if (amh._losted && amh._checkLost)
			{
				amh.throw_lost_exception();
			}
#endif
			if (ms > 0)
			{
				bool overtime = false;
				delay_trig(ms, [&overtime, &th]
				{
					overtime = true;
					th();
				});
				push_yield();
				if (overtime)
				{
					return false;
				}
				cancel_delay_trig();
			}
			else if (ms < 0)
			{
				push_yield();
			}
			else
			{
				return false;
			}
#ifdef ENABLE_CHECK_LOST
			if (amh._losted && amh._checkLost)
			{
				amh.throw_lost_exception();
			}
#endif
		}
		return true;
	}

	template <typename TimedHandler, typename DST, typename... Args>
	bool _timed_wait_msg(ActorMsgHandlePush_<Args...>& amh, TimedHandler&& th, DST& dstRec, int ms)
	{
		return _timed_wait_msg(ms, wrap_local_handler(th), amh, dstRec);
	}

	template <typename DST, typename... Args>
	bool _timed_wait_msg(ActorMsgHandlePush_<Args...>& amh, DST& dstRec, int ms)
	{
		return _timed_wait_msg(amh, [this]
		{
			pull_yield();
		}, dstRec, ms);
	}

	template <typename DST, typename... Args>
	void _wait_msg(ActorMsgHandlePush_<Args...>& amh, DST& dstRec)
	{
		_timed_wait_msg(amh, dstRec, -1);
	}
public:
	/*!
	@brief 创建一个消息通知函数
	*/
	template <typename... Args>
	msg_notifer<Args...> make_msg_notifer_to_self(msg_handle<Args...>& amh, bool checkLost = false)
	{
		return amh.make_notifer(this, checkLost);
	}

	/*!
	@brief 创建一个消息通知函数到buddyActor
	*/
	template <typename... Args>
	msg_notifer<Args...> make_msg_notifer_to(const actor_handle& buddyActor, msg_handle<Args...>& amh, bool checkLost = false)
	{
		assert_enter();
		return amh.make_notifer(buddyActor.get(), checkLost);
	}

	template <typename... Args>
	msg_notifer<Args...> make_msg_notifer_to(my_actor* buddyActor, msg_handle<Args...>& amh, bool checkLost = false)
	{
		assert_enter();
		return amh.make_notifer(buddyActor, checkLost);
	}

	template <typename... Args>
	msg_notifer<Args...> make_msg_notifer_to(child_handle& childActor, msg_handle<Args...>& amh, bool checkLost = false)
	{
		assert_enter();
		return amh.make_notifer(childActor.get_actor().get(), checkLost);
	}

	/*!
	@brief 关闭消息通知句柄
	*/
	void close_msg_notifer(msg_handle_base& amh);

	/*!
	@brief 从消息句柄中提取消息
	@param ms 超时时间
	@return 超时完成返回false，成功提取消息返回true
	*/
	template <typename... Args, typename... Outs>
	__yield_interrupt bool timed_wait_msg(int ms, msg_handle<Args...>& amh, Outs&... res)
	{
		assert_enter();
		DstReceiverRef_<types_pck<Args...>, types_pck<Outs...>> dstRec(res...);
		return _timed_wait_msg(amh, dstRec, ms);
	}

	template <typename TimedHandler, typename... Args, typename... Outs>
	__yield_interrupt bool timed_wait_msg(int ms, TimedHandler&& th, msg_handle<Args...>& amh, Outs&... res)
	{
		assert_enter();
		DstReceiverRef_<types_pck<Args...>, types_pck<Outs...>> dstRec(res...);
		return _timed_wait_msg(amh, th, dstRec, ms);
	}

	__yield_interrupt bool timed_wait_msg(int ms, msg_handle<>& amh);

	template <typename TimedHandler>
	__yield_interrupt bool timed_wait_msg(int ms, TimedHandler&& th, msg_handle<>& amh)
	{
		return _timed_wait_msg(ms, wrap_local_handler(th), amh);
	}

	template <typename... Args, typename Handler>
	__yield_interrupt bool timed_wait_msg_invoke(int ms, msg_handle<Args...>& amh, Handler&& h)
	{
		assert_enter();
		DstReceiverBuff_<Args...> dstRec;
		if (_timed_wait_msg(amh, dstRec, ms))
		{
			tuple_invoke(h, std::move(dstRec._dstBuff.get()));
			return true;
		}
		return false;
	}

	template <typename... Args, typename TimedHandler, typename Handler>
	__yield_interrupt bool timed_wait_msg_invoke(int ms, msg_handle<Args...>& amh, TimedHandler&& th, Handler&& h)
	{
		assert_enter();
		DstReceiverBuff_<Args...> dstRec;
		if (_timed_wait_msg(amh, th, dstRec, ms))
		{
			tuple_invoke(h, std::move(dstRec._dstBuff.get()));
			return true;
		}
		return false;
	}

	template <typename... Args, typename... Outs>
	__yield_interrupt bool try_wait_msg(msg_handle<Args...>& amh, Outs&... res)
	{
		return timed_wait_msg(0, amh, res...);
	}

	__yield_interrupt bool try_wait_msg(msg_handle<>& amh);

	template <typename... Args, typename Handler>
	__yield_interrupt bool try_wait_msg_invoke(msg_handle<Args...>& amh, Handler&& h)
	{
		return timed_wait_msg_invoke(0, amh, h);
	}

	/*!
	@brief 从消息句柄中提取消息
	*/
	template <typename... Args, typename... Outs>
	__yield_interrupt void wait_msg(msg_handle<Args...>& amh, Outs&... res)
	{
		timed_wait_msg(-1, amh, res...);
	}

	__yield_interrupt void wait_msg(msg_handle<>& amh);

	template <typename R>
	__yield_interrupt R wait_msg(msg_handle<R>& amh)
	{
		assert_enter();
		DstReceiverBuff_<R> dstRec;
		_wait_msg(amh, dstRec);
		return std::move(std::get<0>(dstRec._dstBuff.get()));
	}

	template <typename... Args, typename Handler>
	__yield_interrupt void wait_msg_invoke(msg_handle<Args...>& amh, Handler&& h)
	{
		timed_wait_msg_invoke<Args...>(-1, amh, h);
	}

	/*!
	@brief 等待并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt void wait_ignore_msg(msg_handle<Args...>& amh)
	{
		timed_wait_ignore_msg(-1, amh);
	}

	/*!
	@brief 尝试弹出并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt bool try_wait_ignore_msg(msg_handle<Args...>& amh)
	{
		return timed_wait_ignore_msg(0, amh);
	}

	/*!
	@brief 在一定时间内尝试弹出并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt bool timed_wait_ignore_msg(int ms, msg_handle<Args...>& amh)
	{
		std::tuple<ignore_msg<Args>...> ignoreMsg;
		return tuple_invoke<bool>(&my_actor::_timed_wait_ignore_msg<Args...>, std::tuple<my_actor*, int&, msg_handle<Args...>&>(this, ms, amh), ignoreMsg);
	}
private:
	template <typename... Args>
	__yield_interrupt static bool _timed_wait_ignore_msg(my_actor* const host, int ms, msg_handle<Args...>& amh, ignore_msg<Args>&... outs)
	{
		return host->timed_wait_msg(ms, amh, outs...);
	}
public:
	/*!
	@brief 创建上下文回调函数(可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_context_as_type(...))
	*/
	template <typename... Args, typename... Outs>
	callback_handler<types_pck<Args...>, types_pck<Outs...>> make_context_as_type(Outs&... outs)
	{
		static_assert(sizeof...(Args) == sizeof...(Outs), "");
		assert_enter();
		return callback_handler<types_pck<Args...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建上下文回调函数(可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_context(...))
	*/
	template <typename... Outs>
	callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_context(Outs&... outs)
	{
		assert_enter();
		return callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建带超时的上下文回调函数(可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_timed_context(...))
	*/
	template <typename... Outs>
	callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_timed_context(int ms, bool& overtime, Outs&... outs)
	{
		assert_enter();
		overtime = false;
		delay_trig(ms, [this, &overtime]
		{
			overtime = true;
			pull_yield();
		});
		return callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, true, outs...);
	}

	/*!
	@brief 创建带超时的上下文回调函数(可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_timed_context(...))
	*/
	template <typename TimedHandler, typename... Outs>
	callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_timed_context(int ms, TimedHandler&& th, Outs&... outs)
	{
		assert_enter();
		delay_trig(ms, std::forward<TimedHandler>(th));
		return callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, true, outs...);
	}

	/*!
	@brief 创建上下文回调函数(可以少传参数，可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_context_as_type(...))
	*/
	template <typename... Args, typename... Outs>
	same_callback_handler<types_pck<Args...>, types_pck<Outs...>> make_same_context_as_type(Outs&... outs)
	{
		static_assert(sizeof...(Args) == sizeof...(Outs), "");
		assert_enter();
		return same_callback_handler<types_pck<Args...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建上下文回调函数(可以少传参数，可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_context(...))
	*/
	template <typename... Outs>
	same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_same_context(Outs&... outs)
	{
		assert_enter();
		return same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建带超时的上下文回调函数(可以少传参数，可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_timed_context(...))
	*/
	template <typename... Outs>
	same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_timed_same_context(int ms, bool& overtime, Outs&... outs)
	{
		assert_enter();
		overtime = false;
		delay_trig(ms, [this, &overtime]
		{
			overtime = true;
			pull_yield();
		});
		return same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, true, outs...);
	}

	/*!
	@brief 创建带超时的上下文回调函数(可以少传参数，可以多次触发，但只有第一次有效)，直接作为回调函数使用，async_func(..., Handler self->make_timed_context(...))
	*/
	template <typename TimedHandler, typename... Outs>
	same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_timed_same_context(int ms, TimedHandler&& th, Outs&... outs)
	{
		assert_enter();
		delay_trig(ms, std::forward<TimedHandler>(th));
		return same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, true, outs...);
	}

	/*!
	@brief 创建ASIO库上下文回调函数(只能触发一次)，直接作为回调函数使用，async_func(..., Handler self->make_asio_context_as_type(...))
	*/
	template <typename... Args, typename... Outs>
	asio_callback_handler<types_pck<Args...>, types_pck<Outs...>> make_asio_context_as_type(Outs&... outs)
	{
		static_assert(sizeof...(Args) == sizeof...(Outs), "");
		assert_enter();
		return asio_callback_handler<types_pck<Args...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建ASIO库上下文回调函数(只能触发一次)，直接作为回调函数使用，async_func(..., Handler self->make_asio_context(...))
	*/
	template <typename... Outs>
	asio_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_asio_context(Outs&... outs)
	{
		assert_enter();
		return asio_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建带超时的ASIO库上下文回调函数(只能触发一次)，直接作为回调函数使用，async_func(..., Handler self->make_asio_timed_context(...))
	*/
	template <typename TimedHandler, typename... Outs>
	asio_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_asio_timed_context(int ms, TimedHandler&& th, Outs&... outs)
	{
		assert_enter();
		delay_trig(ms, std::forward<TimedHandler>(th));
		return asio_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, true, outs...);
	}

	/*!
	@brief 创建ASIO库上下文回调函数(可以少传参数，只能触发一次)，直接作为回调函数使用，async_func(..., Handler self->make_asio_context_as_type(...))
	*/
	template <typename... Args, typename... Outs>
	asio_same_callback_handler<types_pck<Args...>, types_pck<Outs...>> make_asio_same_context_as_type(Outs&... outs)
	{
		static_assert(sizeof...(Args) == sizeof...(Outs), "");
		assert_enter();
		return asio_same_callback_handler<types_pck<Args...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建ASIO库上下文回调函数(可以少传参数，只能触发一次)，直接作为回调函数使用，async_func(..., Handler self->make_asio_context(...))
	*/
	template <typename... Outs>
	asio_same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_asio_same_context(Outs&... outs)
	{
		assert_enter();
		return asio_same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, false, outs...);
	}

	/*!
	@brief 创建带超时的ASIO库上下文回调函数(可以少传参数，只能触发一次)，直接作为回调函数使用，async_func(..., Handler self->make_asio_timed_context(...))
	*/
	template <typename TimedHandler, typename... Outs>
	asio_same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_asio_timed_same_context(int ms, TimedHandler&& th, Outs&... outs)
	{
		assert_enter();
		delay_trig(ms, std::forward<TimedHandler>(th));
		return asio_same_callback_handler<types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, true, outs...);
	}

	/*!
	@brief 创建上下文回调函数(Handler将在回调内部直接执行，可能触发线程安全问题)，直接作为回调函数使用，async_func(..., Handler self->make_on_context(...))
	*/
	template <typename R = void, typename Handler>
	on_callback_handler<Handler, R> make_on_callback_context(Handler&& handler)
	{
		assert_enter();
		return on_callback_handler<Handler, R>(this, std::forward<Handler>(handler));
	}

	/*!
	@brief 创建同步上下文回调函数，直接作为回调函数使用，async_func(..., Handler self->make_sync_context(sync_result, ...))
	*/
	template <typename R = void, typename... Args, typename... Outs>
	sync_cb_handler<R, types_pck<Args...>, types_pck<Outs...>> make_sync_context_as_type(sync_result<R>& res, Outs&... outs)
	{
		static_assert(sizeof...(Args) == sizeof...(Outs), "");
		assert_enter();
		return sync_cb_handler<R, types_pck<Args...>, types_pck<Outs...>>(this, res, outs...);
	}

	template <typename R = void, typename... Outs>
	sync_cb_handler<R, types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>> make_sync_context(sync_result<R>& res, Outs&... outs)
	{
		assert_enter();
		return sync_cb_handler<R, types_pck<typename check_stack_obj_type<Outs>::type...>, types_pck<Outs...>>(this, res, outs...);
	}

	/*!
	@brief 创建一个消息触发函数，只有一次触发有效
	*/
	template <typename... Args>
	trig_notifer<Args...> make_trig_notifer_to_self(trig_handle<Args...>& ath, bool checkLost = false)
	{
		return ath.make_notifer(this, checkLost);
	}

	/*!
	@brief 创建一个消息触发函数到buddyActor，只有一次触发有效
	*/
	template <typename... Args>
	trig_notifer<Args...> make_trig_notifer_to(const actor_handle& buddyActor, trig_handle<Args...>& ath, bool checkLost = false)
	{
		assert_enter();
		return ath.make_notifer(buddyActor.get(), checkLost);
	}

	template <typename... Args>
	trig_notifer<Args...> make_trig_notifer_to(my_actor* buddyActor, trig_handle<Args...>& ath, bool checkLost = false)
	{
		assert_enter();
		return ath.make_notifer(buddyActor, checkLost);
	}

	template <typename... Args>
	trig_notifer<Args...> make_trig_notifer_to(child_handle& childActor, trig_handle<Args...>& ath, bool checkLost = false)
	{
		assert_enter();
		return ath.make_notifer(childActor.get_actor().get(), checkLost);
	}

	/*!
	@brief 关闭消息触发句柄
	*/
	void close_trig_notifer(msg_handle_base& ath);

	/*!
	@brief 从触发句柄中提取消息
	@param ms 超时时间
	@return 超时完成返回false，成功提取消息返回true
	*/
	template <typename... Args, typename... Outs>
	__yield_interrupt bool timed_wait_trig(int ms, trig_handle<Args...>& ath, Outs&... res)
	{
		assert_enter();
		DstReceiverRef_<types_pck<Args...>, types_pck<Outs...>> dstRec(res...);
		return _timed_wait_msg(ath, dstRec, ms);
	}

	template <typename TimedHandler, typename... Args, typename... Outs>
	__yield_interrupt bool timed_wait_trig(int ms, TimedHandler&& th, trig_handle<Args...>& ath, Outs&... res)
	{
		assert_enter();
		DstReceiverRef_<types_pck<Args...>, types_pck<Outs...>> dstRec(res...);
		return _timed_wait_msg(ath, th, dstRec, ms);
	}

	__yield_interrupt bool timed_wait_trig(int ms, trig_handle<>& ath);

	template <typename TimedHandler>
	__yield_interrupt bool timed_wait_trig(int ms, TimedHandler&& th, trig_handle<>& ath)
	{
		return _timed_wait_msg(ms, wrap_local_handler(th), ath);
	}

	template <typename... Args, typename Handler>
	__yield_interrupt bool timed_wait_trig_invoke(int ms, trig_handle<Args...>& ath, Handler&& h)
	{
		assert_enter();
		DstReceiverBuff_<Args...> dstRec;
		if (_timed_wait_msg(ath, dstRec, ms))
		{
			tuple_invoke(h, std::move(dstRec._dstBuff.get()));
			return true;
		}
		return false;
	}

	template <typename... Args, typename TimedHandler, typename Handler>
	__yield_interrupt bool timed_wait_trig_invoke(int ms, trig_handle<Args...>& ath, TimedHandler&& th, Handler&& h)
	{
		assert_enter();
		DstReceiverBuff_<Args...> dstRec;
		if (_timed_wait_msg(ath, th, dstRec, ms))
		{
			tuple_invoke(h, std::move(dstRec._dstBuff.get()));
			return true;
		}
		return false;
	}

	template <typename... Args, typename... Outs>
	__yield_interrupt bool try_wait_trig(trig_handle<Args...>& ath, Outs&... res)
	{
		return timed_wait_trig(0, ath, res...);
	}

	__yield_interrupt bool try_wait_trig(trig_handle<>& ath);

	template <typename... Args, typename Handler>
	__yield_interrupt bool try_wait_trig_invoke(trig_handle<Args...>& ath, Handler&& h)
	{
		return timed_wait_trig_invoke(0, ath, h);
	}

	/*!
	@brief 从触发句柄中提取消息
	*/
	template <typename... Args, typename... Outs>
	__yield_interrupt void wait_trig(trig_handle<Args...>& ath, Outs&... res)
	{
		timed_wait_trig(-1, ath, res...);
	}

	__yield_interrupt void wait_trig(trig_handle<>& ath);

	template <typename R>
	__yield_interrupt R wait_trig(trig_handle<R>& ath)
	{
		assert_enter();
		DstReceiverBuff_<R> dstRec;
		_wait_msg(ath, dstRec);
		return std::move(std::get<0>(dstRec._dstBuff.get()));
	}

	template <typename... Args, typename Handler>
	__yield_interrupt void wait_trig_invoke(trig_handle<Args...>& ath, Handler&& h)
	{
		timed_wait_trig_invoke<Args...>(-1, ath, h);
	}

	/*!
	@brief 等待并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt void wait_ignore_trig(trig_handle<Args...>& ath)
	{
		timed_wait_ignore_trig(-1, ath);
	}

	/*!
	@brief 尝试弹出并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt bool try_wait_ignore_trig(trig_handle<Args...>& ath)
	{
		return timed_wait_ignore_trig(0, ath);
	}

	/*!
	@brief 在一定时间内尝试弹出并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt bool timed_wait_ignore_trig(int ms, trig_handle<Args...>& ath)
	{
		std::tuple<ignore_msg<Args>...> ignoreMsg;
		return tuple_invoke<bool>(&my_actor::_timed_wait_ignore_trig<Args...>, std::tuple<my_actor*, int&, trig_handle<Args...>&>(this, ms, ath), ignoreMsg);
	}

	/*!
	@brief 等待触发消息
	*/
	__yield_interrupt void wait_trig_sign(int id);

	/*!
	@brief 超时等待触发消息
	*/
	__yield_interrupt bool timed_wait_trig_sign(int ms, int id);

	template <typename TimedHandler>
	__yield_interrupt bool timed_wait_trig_sign(int ms, int id, TimedHandler&& th)
	{
		return _timed_wait_trig_sign(ms, id, wrap_local_handler(th));
	}

	/*!
	@brief 尝试等待触发消息
	*/
	bool try_wait_trig_sign(int id);
private:
	template <typename... Args>
	__yield_interrupt static bool _timed_wait_ignore_trig(my_actor* const host, int ms, trig_handle<Args...>& ath, ignore_msg<Args>&... outs)
	{
		return host->timed_wait_trig(ms, ath, outs...);
	}
private:
	/*!
	@brief 寻找出与模板参数类型匹配的消息池
	*/
	template <typename... Args>
	static std::shared_ptr<msg_pool_status::pck<Args...> > msg_pool_pck(const int id, my_actor* const host, const bool make = true)
	{
		assert(id >= 0 && id < 256);
		typedef msg_pool_status::pck<Args...> pck_type;
		msg_pool_status::id_key typeID(type_hash<Args...>::hash_code(), id);
		if (make)
		{
			auto& res = host->_msgPoolStatus._msgTypeMap.insert(make_pair(typeID, std::shared_ptr<pck_type>())).first->second;
			if (!res)
			{
				res = std::make_shared<pck_type>(host);
			}
			assert(std::dynamic_pointer_cast<pck_type>(res));
			return std::static_pointer_cast<pck_type>(res);
		}
		auto it = host->_msgPoolStatus._msgTypeMap.find(typeID);
		if (it != host->_msgPoolStatus._msgTypeMap.end())
		{
			assert(std::dynamic_pointer_cast<pck_type>(it->second));
			return std::static_pointer_cast<pck_type>(it->second);
		}
		return std::shared_ptr<pck_type>();
	}

	template <typename... Args>
	static void disconnect_pump(my_actor* const host, const std::shared_ptr<MsgPool_<Args...>>& msgPool, const std::shared_ptr<MsgPump_<Args...>>& msgPump)
	{
		typedef typename MsgPump_<Args...>::msg_type msg_type;

		if (msgPool)
		{
			host->send(msgPool->_strand, [&]
			{
				assert(!msgPool->_msgPump || msgPool->_msgPump == msgPump);
				msgPool->disconnect();
			});
		}
		if (msgPump)
		{
			stack_obj<msg_type> suck;
			bool losted = false;
			host->send(msgPump->_strand, [&msgPump, &suck, &losted]
			{
				msgPump->backflow(suck, losted);
				msgPump->clear();
			});
			if (suck.has() || losted)
			{
				assert(msgPool);
				host->send(msgPool->_strand, [&msgPool, &suck]
				{
					msgPool->backflow(suck);
				});
			}
		}
	}

	/*!
	@brief 清除消息代理链
	*/
	template <typename... Args>
	static void clear_msg_list(my_actor* const host, const std::shared_ptr<msg_pool_status::pck<Args...>>& msgPck)
	{
		size_t stackl = 0;
		msg_pool_status::pck<Args...>* uStack[16];
		msg_pool_status::pck<Args...>* pckIt = msgPck.get();
		while (pckIt->_next)
		{
			assert(!pckIt->_next->_isHead);
			pckIt->_msgPool.reset();
			pckIt = pckIt->_next.get();
			pckIt->lock(host);
			assert(stackl < 15);
			uStack[stackl++] = pckIt;
		}
		if (!pckIt->is_closed())
		{
			auto& msgPool_ = pckIt->_msgPool;
			disconnect_pump<Args...>(host, msgPool_, pckIt->_msgPump);
			msgPool_.reset();
		}
		while (stackl)
		{
			uStack[--stackl]->unlock(host);
		}
	}

	/*!
	@brief 更新消息代理链
	*/
	template <typename... Args>
	void update_msg_list(const std::shared_ptr<msg_pool_status::pck<Args...>>& msgPck, const std::shared_ptr<MsgPool_<Args...>>& newPool)
	{
		typedef typename MsgPool_<Args...>::pump_handler pump_handler;

		size_t stackl = 0;
		msg_pool_status::pck<Args...>* uStack[16];
		msg_pool_status::pck<Args...>* pckIt = msgPck.get();
		while (pckIt->_next)
		{
			assert(!pckIt->_next->_isHead);
			pckIt->_msgPool = newPool;
			pckIt = pckIt->_next.get();
			pckIt->lock(this);
			assert(stackl < 15);
			uStack[stackl++] = pckIt;
		}
		if (!pckIt->is_closed())
		{
			auto& msgPool_ = pckIt->_msgPool;
			auto& msgPump_ = pckIt->_msgPump;
			disconnect_pump<Args...>(this, msgPool_, msgPump_);
			msgPool_ = newPool;
			if (pckIt->_msgPump)
			{
				if (msgPool_)
				{
					auto ph = send<pump_handler>(msgPool_->_strand, [&pckIt]()->pump_handler
					{
						return pckIt->_msgPool->connect_pump(pckIt->_msgPump);
					});
					send(msgPump_->_strand, [&msgPump_, &ph]
					{
						msgPump_->connect(std::move(ph));
					});
				}
				else
				{
					send(msgPump_->_strand, [&msgPump_]
					{
						msgPump_->clear();
					});
				}
			}
		}
		while (stackl)
		{
			uStack[--stackl]->unlock(this);
		}
	}
private:
	/*!
	@brief 把本Actor内消息由伙伴Actor代理处理
	*/
	template <typename... Args>
	__yield_interrupt bool msg_agent_to(const int id, const actor_handle& childActor)
	{
		typedef std::shared_ptr<msg_pool_status::pck<Args...>> pck_type;

		assert_enter();
		assert(childActor);
		{
			auto isSelf = childActor->parent_actor();
			if (!isSelf || isSelf->self_id() != self_id())
			{
				assert(false);
				return false;
			}
		}
		lock_suspend();
		lock_quit();
		pck_type msgPck = msg_pool_pck<Args...>(id, this);
		msgPck->lock(this);
		pck_type childPck;
		while (true)
		{
			childPck = send<pck_type>(childActor->self_strand(), [id, &childActor]()->pck_type
			{
				if (!childActor->_quited)
				{
					return my_actor::msg_pool_pck<Args...>(id, childActor.get());
				}
				return pck_type();
			});
			if (!childPck || msgPck->_next == childPck)
			{
				msgPck->unlock(this);
				unlock_quit();
				unlock_suspend();
				return false;
			}
			childPck->lock(this);
			if (!childPck->is_closed())
			{
				break;
			}
			childPck->unlock(this);
		}
		auto msgPool = msgPck->_msgPool;
		auto& next_ = msgPck->_next;
		if (next_)
		{
			next_->lock(this);
			assert(!next_->_isHead);
			clear_msg_list<Args...>(this, next_);
		}
		else
		{
			clear_msg_list<Args...>(this, msgPck);
			msgPck->_msgPool = msgPool;
		}
		update_msg_list<Args...>(childPck, msgPool);
		if (next_)
		{
			next_->_isHead = true;
			next_->unlock(this);
		}
		next_ = childPck;
		childPck->_isHead = false;
		childPck->unlock(this);
		msgPck->unlock(this);
		unlock_quit();
		unlock_suspend();
		return true;
	}
public:
	template <typename... Args>
	__yield_interrupt bool msg_agent_to(const int id, child_handle& childActor)
	{
		return msg_agent_to<Args...>(id, childActor.get_actor());
	}

	template <typename... Args>
	__yield_interrupt bool msg_agent_to(child_handle& childActor)
	{
		return msg_agent_to<Args...>(0, childActor.get_actor());
	}
public:
	/*!
	@brief 把消息指定到一个特定Actor函数体去处理
	@return 返回处理该消息的子Actor句柄
	*/
	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(const int id, const shared_strand& strand, bool autoRun, Handler&& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		actor_handle childActor = my_actor::create(strand, std::bind([id](Handler& agentActor, my_actor* self)
		{
			msg_pump_handle<Args...> pump = my_actor::_connect_msg_pump<Args...>(id, self, false);
			agentActor(self, pump);
		}, std::forward<Handler>(agentActor), __1), stackSize);
		childActor->_parentActor = shared_from_this();
		msg_agent_to<Args...>(id, childActor);
		if (autoRun)
		{
			childActor->run();
		}
		return child_handle(std::move(childActor));
	}

	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(const int id, const shared_strand& strand, bool autoRun, AutoStackMsgAgentActor_<Handler>&& wrapActor)
	{
		static_assert(std::is_reference<Handler>::value, "");
		actor_handle childActor = my_actor::create(strand, __auto_stack_actor(std::bind([id](Handler& agentActor, my_actor* self)
		{
			msg_pump_handle<Args...> pump = my_actor::_connect_msg_pump<Args...>(id, self, false);
			agentActor(self, pump);
		}, (Handler)wrapActor._h, __1), wrapActor._stackSize, wrapActor._key));
		childActor->_parentActor = shared_from_this();
		msg_agent_to<Args...>(id, childActor);
		if (autoRun)
		{
			childActor->run();
		}
		return child_handle(std::move(childActor));
	}

	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(const int id, bool autoRun, Handler&& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<Args...>(id, self_strand(), autoRun, std::forward<Handler>(agentActor), stackSize);
	}

	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(const shared_strand& strand, bool autoRun, Handler&& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<Args...>(0, strand, autoRun, std::forward<Handler>(agentActor), stackSize);
	}

	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(bool autoRun, Handler&& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<Args...>(0, self_strand(), autoRun, std::forward<Handler>(agentActor), stackSize);
	}

	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(const int id, bool autoRun, AutoStackMsgAgentActor_<Handler>&& wrapActor)
	{
		return msg_agent_to_actor<Args...>(id, self_strand(), autoRun, std::move(wrapActor));
	}

	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(const shared_strand& strand, bool autoRun, AutoStackMsgAgentActor_<Handler>&& wrapActor)
	{
		return msg_agent_to_actor<Args...>(0, strand, autoRun, std::move(wrapActor));
	}

	template <typename... Args, typename Handler>
	__yield_interrupt child_handle msg_agent_to_actor(bool autoRun, AutoStackMsgAgentActor_<Handler>&& wrapActor)
	{
		return msg_agent_to_actor<Args...>(0, self_strand(), autoRun, std::move(wrapActor));
	}
public:
	/*!
	@brief 断开伙伴代理该消息
	*/
	template <typename... Args>
	__yield_interrupt void msg_agent_off(const int id = 0)
	{
		assert_enter();
		auto msgPck = msg_pool_pck<Args...>(id, this, false);
		if (msgPck)
		{
			lock_suspend();
			lock_quit();
			auto& next_ = msgPck->_next;
			msgPck->lock(this);
			if (next_)
			{
				next_->lock(this);
				assert(!next_->_isHead);
				clear_msg_list<Args...>(this, next_);
				next_->_isHead = true;
				next_->unlock(this);
				next_.reset();
			}
			msgPck->unlock(this);
			unlock_quit();
			unlock_suspend();
		}
	}

	/*!
	@brief 重置消息管道，之前该类型下connect_msg_notifer到本地的通知句柄将永久失效，本地必须重新connect_msg_pump，发送者必须重新connect_msg_notifer才能重新开始
	*/
	template <typename... Args>
	__yield_interrupt bool reset_msg_pipe(const int id = 0)
	{
		assert_enter();
		assert(id >= 0 && id < 256);
		typedef msg_pool_status::pck<Args...> pck_type;
		msg_pool_status::id_key typeID(type_hash<Args...>::hash_code(), id);
		auto it = _msgPoolStatus._msgTypeMap.find(typeID);
		if (_msgPoolStatus._msgTypeMap.end() != it)
		{
			lock_suspend();
			lock_quit();
			assert(std::dynamic_pointer_cast<pck_type>(it->second));
			std::shared_ptr<pck_type> msgPck = std::static_pointer_cast<pck_type>(it->second);
			msgPck->lock(this);
			auto msgPool = msgPck->_msgPool;
			clear_msg_list<Args...>(this, msgPck);
			msgPck->_msgPool = msgPool;
			msgPck->clear();
			_msgPoolStatus._msgTypeMap.erase(it);
			msgPck->unlock(this);
			unlock_quit();
			unlock_suspend();
			return true;
		}
		return false;
	}
public:
	/*!
	@brief 连接消息通知到一个伙伴Actor，该Actor必须是子Actor或根Actor
	@param strand 消息调度器
	@param id 相同类型消息id
	@param makeNew false 如果该消息是个根消息就成功，否则失败；true 强制创建新的通知，之前的通知和代理将失效
	@param fixedSize 消息队列内存池长度
	@warning 如果 makeNew = false 且该节点为父的代理，将创建失败
	@return 消息通知函数
	*/
	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(const shared_strand& strand, const int id, const actor_handle& buddyActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		typedef MsgPool_<Args...> pool_type;
		typedef typename pool_type::pump_handler pump_handler;
		typedef std::shared_ptr<msg_pool_status::pck<Args...>> pck_type;

#ifndef ENABLE_CHECK_LOST
		assert(!chekcLost);
#endif
		assert_enter();
#if (_DEBUG || DEBUG)
		{
			assert(!buddyActor->parent_actor() || buddyActor->parent_actor()->self_id() == self_id());
			auto selfParent = parent_actor();
			while (selfParent)
			{
				assert(selfParent->self_id() != buddyActor->self_id());
				selfParent = selfParent->parent_actor();
			}
		}
#endif
		lock_suspend();
		lock_quit();
		pck_type msgPck = msg_pool_pck<Args...>(id, this);
		msgPck->lock(this);
		pck_type buddyPck;
		while (true)
		{
			buddyPck = send<pck_type>(buddyActor->self_strand(), [id, &buddyActor]()->pck_type
			{
				if (!buddyActor->_quited)
				{
					return my_actor::msg_pool_pck<Args...>(id, buddyActor.get());
				}
				return pck_type();
			});
			if (!buddyPck)
			{
				msgPck->unlock(this);
				unlock_quit();
				unlock_suspend();
				return post_actor_msg<Args...>();
			}
			buddyPck->lock(this);
			if (!buddyPck->is_closed())
			{
				break;
			}
			buddyPck->unlock(this);
		}
		if (makeNew)
		{
			if (buddyPck->_msgPool)
			{
				buddyPck->_msgPool->_closed = true;
			}
			auto newPool = pool_type::make(strand, fixedSize);
			update_msg_list<Args...>(buddyPck, newPool);
			buddyPck->_isHead = true;
			buddyPck->unlock(this);
			if (msgPck->_next == buddyPck)
			{//之前的代理将被取消
				msgPck->_next.reset();
				if (msgPck->_msgPump)
				{
					auto& msgPump_ = msgPck->_msgPump;
					auto& msgPool_ = msgPck->_msgPool;
					if (msgPool_)
					{
						msgPump_->connect(this->send<pump_handler>(msgPool_->_strand, [&msgPck]()->pump_handler
						{
							return msgPck->_msgPool->connect_pump(msgPck->_msgPump);
						}));
					}
					else
					{
						msgPump_->clear();
					}
				}
			}
#ifdef ENABLE_CHECK_LOST
			std::shared_ptr<CheckPumpLost_> autoCheckLost;
			if (chekcLost)
			{
				autoCheckLost = ActorFunc_::new_check_pump_lost(buddyActor, newPool.get());
				newPool->_weakCheckLost = autoCheckLost;
			}
			msgPck->unlock(this);
			unlock_quit();
			unlock_suspend();
			return post_actor_msg<Args...>(newPool, buddyActor, autoCheckLost);
#else
			msgPck->unlock(this);
			unlock_quit();
			unlock_suspend();
			return post_actor_msg<Args...>(newPool, buddyActor);
#endif
		}
		if (buddyPck->_isHead)
		{
			auto& buddyPool = buddyPck->_msgPool;
			if (!buddyPool)
			{
				buddyPool = pool_type::make(strand, fixedSize);
				update_msg_list<Args...>(buddyPck, buddyPool);
			}
#ifdef ENABLE_CHECK_LOST
			std::shared_ptr<CheckPumpLost_> autoCheckLost;
			if (chekcLost)
			{
				autoCheckLost = buddyPool->_weakCheckLost.lock();
				if (!autoCheckLost)
				{
					autoCheckLost = ActorFunc_::new_check_pump_lost(buddyActor, buddyPool.get());
					buddyPool->_weakCheckLost = autoCheckLost;
				}
			}
			buddyPck->unlock(this);
			msgPck->unlock(this);
			unlock_quit();
			unlock_suspend();
			return post_actor_msg<Args...>(buddyPool, buddyActor, std::move(autoCheckLost));
#else
			buddyPck->unlock(this);
			msgPck->unlock(this);
			unlock_quit();
			unlock_suspend();
			return post_actor_msg<Args...>(buddyPool, buddyActor);
#endif
		}
		buddyPck->unlock(this);
		msgPck->unlock(this);
		unlock_quit();
		unlock_suspend();
		return post_actor_msg<Args...>();
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(const shared_strand& strand, const int id, child_handle& childActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(strand, id, childActor.get_actor(), chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(const shared_strand& strand, const actor_handle& buddyActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(strand, 0, buddyActor, chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(const shared_strand& strand, child_handle& childActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(strand, 0, childActor.get_actor(), makeNew, chekcLost, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(const int id, const actor_handle& buddyActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(buddyActor->self_strand(), id, buddyActor, chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(const int id, child_handle& childActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(childActor->self_strand(), id, childActor.get_actor(), chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(const actor_handle& buddyActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(buddyActor->self_strand(), 0, buddyActor, chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to(child_handle& childActor, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(childActor->self_strand(), 0, childActor.get_actor(), chekcLost, makeNew, fixedSize);
	}

	/*!
	@brief 连接消息通知到自己的Actor
	@param strand 消息调度器
	@param id 相同类型消息id
	@param makeNew false 如果存在返回之前，否则创建新的通知；true 强制创建新的通知，之前的将失效，且断开与buddyActor的关联
	@param fixedSize 消息队列内存池长度
	@warning 如果该节点为父的代理，那么将创建失败
	@return 消息通知函数
	*/
	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to_self(const shared_strand& strand, const int id, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<Args...>(strand, id, shared_from_this(), chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to_self(const shared_strand& strand, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<Args...>(strand, 0, chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to_self(const int id, bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<Args...>(self_strand(), id, chekcLost, makeNew, fixedSize);
	}

	template <typename... Args>
	__yield_interrupt post_actor_msg<Args...> connect_msg_notifer_to_self(bool chekcLost = false, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<Args...>(self_strand(), 0, chekcLost, makeNew, fixedSize);
	}

	/*!
	@brief 创建一个消息通知函数，在该Actor所依赖的ios无关线程中使用，且在该Actor调用 run() 之前
	@param strand 消息调度器
	@param id 相同类型消息id
	@param fixedSize 消息队列内存池长度
	@return 消息通知函数
	*/
	template <typename... Args>
	post_actor_msg<Args...> connect_msg_notifer(const shared_strand& strand, const int id, bool chekcLost = false, size_t fixedSize = 16)
	{
		typedef post_actor_msg<Args...> post_type;

		return _strand->sync_invoke<post_type>([&]()->post_type
		{
			typedef MsgPool_<Args...> pool_type;
			if (!parent_actor() && !_started && !_quited)
			{
				auto msgPck = msg_pool_pck<Args...>(id, this);
				msgPck->_msgPool = pool_type::make(strand, fixedSize);
#ifdef ENABLE_CHECK_LOST
				std::shared_ptr<CheckPumpLost_> autoCheckLost;
				if (chekcLost)
				{
					autoCheckLost = msgPck->_msgPool->_weakCheckLost.lock();
					if (!autoCheckLost)
					{
						autoCheckLost = ActorFunc_::new_check_pump_lost(shared_from_this(), msgPck->_msgPool.get());
						msgPck->_msgPool->_weakCheckLost = autoCheckLost;
					}
				}
				return post_type(msgPck->_msgPool, shared_from_this(), std::move(autoCheckLost));
#else
				return post_type(msgPck->_msgPool, shared_from_this());
#endif
			}
			assert(false);
			return post_type();
		});
	}

	template <typename... Args>
	post_actor_msg<Args...> connect_msg_notifer(const shared_strand& strand, bool chekcLost = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer<Args...>(strand, 0, chekcLost, fixedSize);
	}

	template <typename... Args>
	post_actor_msg<Args...> connect_msg_notifer(const int id, bool chekcLost = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer<Args...>(self_strand(), id, chekcLost, fixedSize);
	}

	template <typename... Args>
	post_actor_msg<Args...> connect_msg_notifer(bool chekcLost = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer<Args...>(self_strand(), 0, chekcLost, fixedSize);
	}

	template <typename... Args>
	post_actor_msg<Args...> connect_msg_notifer_unsafe(const int id, bool chekcLost = false, size_t fixedSize = 16)
	{
		typedef post_actor_msg<Args...> post_type;
		typedef MsgPool_<Args...> pool_type;
		assert(self_strand()->in_this_ios());
		assert(self_strand()->sync_safe());
		if (!_parentActor && !_started && !_quited)
		{
			auto msgPck = msg_pool_pck<Args...>(id, this);
			msgPck->_msgPool = pool_type::make(self_strand(), fixedSize);
#ifdef ENABLE_CHECK_LOST
			std::shared_ptr<CheckPumpLost_> autoCheckLost;
			if (chekcLost)
			{
				autoCheckLost = msgPck->_msgPool->_weakCheckLost.lock();
				if (!autoCheckLost)
				{
					autoCheckLost = ActorFunc_::new_check_pump_lost(shared_from_this(), msgPck->_msgPool.get());
					msgPck->_msgPool->_weakCheckLost = autoCheckLost;
				}
			}
			return post_type(msgPck->_msgPool, shared_from_this(), std::move(autoCheckLost));
#else
			return post_type(msgPck->_msgPool, shared_from_this());
#endif
		}
		assert(false);
		return post_type();
	}

	template <typename... Args>
	post_actor_msg<Args...> connect_msg_notifer_unsafe(bool chekcLost = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_unsafe<Args...>(0, chekcLost, fixedSize);
	}
	//////////////////////////////////////////////////////////////////////////

	/*!
	@brief 连接消息泵到消息池
	@return 返回消息泵句柄
	*/
	template <typename... Args>
	__yield_interrupt msg_pump_handle<Args...> connect_msg_pump(const int id = 0, bool checkLost = false)
	{
		return _connect_msg_pump<Args...>(id, this, checkLost);
	}

	template <typename... Args>
	__yield_interrupt msg_pump_handle<Args...> connect_msg_pump(bool checkLost)
	{
		return _connect_msg_pump<Args...>(0, this, checkLost);
	}
private:
	template <typename... Args>
	static msg_pump_handle<Args...> _connect_msg_pump(const int id, my_actor* const host, bool checkLost)
	{
		typedef MsgPump_<Args...> pump_type;
		typedef MsgPool_<Args...> pool_type;
		typedef typename pool_type::pump_handler pump_handler;

		host->assert_enter();
		host->lock_suspend();
		host->lock_quit();
		auto msgPck = msg_pool_pck<Args...>(id, host);
		auto& msgPump_ = msgPck->_msgPump;
		auto& msgPool_ = msgPck->_msgPool;
		msgPck->lock(host);
		if (msgPck->_next)
		{
			msgPck->_next->lock(host);
			clear_msg_list<Args...>(host, msgPck->_next);
			msgPck->_next->unlock(host);
			msgPck->_next.reset();
		}
		else
		{
			disconnect_pump<Args...>(host, msgPool_, msgPump_);
		}
		if (!msgPump_)
		{
			msgPump_ = pump_type::make(host, checkLost);
		}
		else
		{
			msgPump_->_checkLost = checkLost;
		}
		if (msgPool_)
		{
			msgPump_->connect(host->send<pump_handler>(msgPool_->_strand, [&msgPck]()->pump_handler
			{
				return msgPck->_msgPool->connect_pump(msgPck->_msgPump);
			}));
		}
		else
		{
			msgPump_->clear();
		}
		msgPck->unlock(host);
		msg_pump_handle<Args...> mh;
		mh._handle = msgPump_.get();
		mh._id = id;
		DEBUG_OPERATION(mh._closed = msgPump_->_closed);
		host->unlock_quit();
		host->unlock_suspend();
		return mh;
	}
private:
	bool _timed_pump_msg(int ms, const wrap_local_handler_face<void()>& th, bool checkDis, const msg_pump_handle<>& pump);

	template <typename DST, typename... Args>
	bool _timed_pump_msg(int ms, const wrap_local_handler_face<void()>& th, bool checkDis, const msg_pump_handle<Args...>& pump, DST& dstRec)
	{
		assert(!pump.check_closed());
		assert(pump.get()->_hostActor && pump.get()->_hostActor->self_id() == self_id());
		if (!pump.get()->read_msg(dstRec))
		{
			BREAK_OF_SCOPE_EXEC(pump.get()->stop_waiting());
			if (checkDis && pump.get()->isDisconnected())
			{
				throw typename msg_pump_handle<Args...>::pump_disconnected(pump.get_id());
			}
#ifdef ENABLE_CHECK_LOST
			if (pump.get()->_losted && pump.get()->_checkLost)
			{
				throw typename msg_pump_handle<Args...>::lost_exception(pump.get_id());
			}
#endif
			pump.get()->_checkDis = checkDis;
			if (ms >= 0)
			{
				bool overtime = false;
				delay_trig(ms, [&overtime, &th]
				{
					overtime = true;
					th();
				});
				push_yield();
				if (overtime)
				{
					return false;
				}
				cancel_delay_trig();
			}
			else
			{
				push_yield();
			}
			if (pump.get()->_checkDis)
			{
				assert(checkDis);
				throw typename msg_pump_handle<Args...>::pump_disconnected(pump.get_id());
			}
#ifdef ENABLE_CHECK_LOST
			if (pump.get()->_losted && pump.get()->_checkLost)
			{
				throw typename msg_pump_handle<Args...>::lost_exception(pump.get_id());
			}
#endif
		}
		return true;
	}

	template <typename TimedHandler, typename DST, typename... Args>
	bool _timed_pump_msg(const msg_pump_handle<Args...>& pump, TimedHandler&& th, DST& dstRec, int ms, bool checkDis)
	{
		return _timed_pump_msg(ms, wrap_local_handler(th), checkDis, pump, dstRec);
	}

	template <typename DST, typename... Args>
	bool _timed_pump_msg(const msg_pump_handle<Args...>& pump, DST& dstRec, int ms, bool checkDis)
	{
		return _timed_pump_msg(pump, [this]
		{
			pull_yield();
		}, dstRec, ms, checkDis);
	}

	template <typename DST, typename... Args>
	bool _try_pump_msg(const msg_pump_handle<Args...>& pump, DST& dstRec, bool checkDis)
	{
		assert(!pump.check_closed());
		assert(pump.get()->_hostActor && pump.get()->_hostActor->self_id() == self_id());
		BREAK_OF_SCOPE_EXEC(pump.get()->stop_waiting());
		if (!pump.get()->try_read(dstRec))
		{
			if (checkDis && pump.get()->isDisconnected())
			{
				throw typename msg_pump_handle<Args...>::pump_disconnected(pump.get_id());
			}
#ifdef ENABLE_CHECK_LOST
			if (pump.get()->_losted && pump.get()->_checkLost)
			{
				throw typename msg_pump_handle<Args...>::lost_exception(pump.get_id());
			}
#endif
			return false;
		}
		return true;
	}

	template <typename DST, typename... Args>
	void _pump_msg(const msg_pump_handle<Args...>& pump, DST& dstRec, bool checkDis)
	{
		_timed_pump_msg(pump, dstRec, -1, checkDis);
	}

	template <typename... Args>
	bool _timed_wait_connect(const msg_pump_handle<Args...>& pump, int ms)
	{
		assert(!pump.check_closed());
		assert(pump.get()->_hostActor && pump.get()->_hostActor->self_id() == self_id());
		if (!pump.is_connected())
		{
			if (0 == ms)
			{
				return false;
			}
			BREAK_OF_SCOPE_EXEC(pump.get()->stop_waiting());
			pump.get()->_waitConnect = true;
			if (ms >= 0)
			{
				bool overtime = false;
				delay_trig(ms, [this, &overtime]
				{
					overtime = true;
					pull_yield();
				});
				push_yield();
				if (overtime)
				{
					return false;
				}
				cancel_delay_trig();
			}
			else
			{
				push_yield();
			}
		}
		return true;
	}
public:

	/*!
	@brief 从消息泵中提取消息
	@param ms 超时时间
	@param checkDis 检测是否被断开连接，是就抛出 pump_disconnected_exception 异常
	@return 超时完成返回false，成功取到消息返回true
	*/
	template <typename... Args, typename... Outs>
	__yield_interrupt bool timed_pump_msg(int ms, bool checkDis, const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		assert_enter();
		DstReceiverRef_<types_pck<Args...>, types_pck<Outs...>> dstRec(res...);
		return _timed_pump_msg(pump, dstRec, ms, checkDis);
	}

	template <typename TimedHandler, typename... Args, typename... Outs>
	__yield_interrupt bool timed_pump_msg(int ms, TimedHandler&& th, bool checkDis, const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		assert_enter();
		DstReceiverRef_<types_pck<Args...>, types_pck<Outs...>> dstRec(res...);
		return _timed_pump_msg(pump, th, dstRec, ms, checkDis);
	}

	template <typename... Args, typename... Outs>
	__yield_interrupt bool timed_pump_msg(int ms, const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		return timed_pump_msg(ms, false, pump, res...);
	}

	template <typename TimedHandler, typename... Args, typename... Outs>
	__yield_interrupt bool timed_pump_msg(int ms, TimedHandler&& th, const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		return timed_pump_msg(ms, th, false, pump, res...);
	}

	__yield_interrupt bool timed_pump_msg(int ms, bool checkDis, const msg_pump_handle<>& pump);

	template <typename TimedHandler>
	__yield_interrupt bool timed_pump_msg(int ms, TimedHandler&& th, bool checkDis, const msg_pump_handle<>& pump)
	{
		return _timed_pump_msg(ms, wrap_local_handler(th), checkDis, pump);
	}

	__yield_interrupt bool timed_pump_msg(int ms, const msg_pump_handle<>& pump);

	template <typename TimedHandler>
	__yield_interrupt bool timed_pump_msg(int ms, TimedHandler&& th, const msg_pump_handle<>& pump)
	{
		return timed_pump_msg(ms, th, false, pump);
	}

	template <typename... Args, typename Handler>
	__yield_interrupt bool timed_pump_msg_invoke(int ms, bool checkDis, const msg_pump_handle<Args...>& pump, Handler&& h)
	{
		assert_enter();
		DstReceiverBuff_<Args...> dstRec;
		if (_timed_pump_msg(pump, dstRec, ms, checkDis))
		{
			tuple_invoke(h, std::move(dstRec._dstBuff.get()));
			return true;
		}
		return false;
	}

	template <typename... Args, typename TimedHandler, typename Handler>
	__yield_interrupt bool timed_pump_msg_invoke(int ms, bool checkDis, const msg_pump_handle<Args...>& pump, TimedHandler&& th, Handler&& h)
	{
		assert_enter();
		DstReceiverBuff_<Args...> dstRec;
		if (_timed_pump_msg(pump, th, dstRec, ms, checkDis))
		{
			tuple_invoke(h, std::move(dstRec._dstBuff.get()));
			return true;
		}
		return false;
	}

	template <typename... Args, typename Handler>
	__yield_interrupt bool timed_pump_msg_invoke(int ms, const msg_pump_handle<Args...>& pump, Handler&& h)
	{
		return timed_pump_msg_invoke(ms, false, pump, h);
	}

	template <typename... Args, typename TimedHandler, typename Handler>
	__yield_interrupt bool timed_pump_msg_invoke(int ms, const msg_pump_handle<Args...>& pump, TimedHandler&& th, Handler&& h)
	{
		return timed_pump_msg_invoke(ms, false, pump, th, h);
	}

	/*!
	@brief 尝试从消息泵中提取消息
	*/
	template <typename... Args, typename... Outs>
	__yield_interrupt bool try_pump_msg(bool checkDis, const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		assert_enter();
		DstReceiverRef_<types_pck<Args...>, types_pck<Outs...>> dstRec(res...);
		return _try_pump_msg(pump, dstRec, checkDis);
	}

	template <typename... Args, typename... Outs>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		return try_pump_msg(false, pump, res...);
	}

	__yield_interrupt bool try_pump_msg(bool checkDis, const msg_pump_handle<>& pump);

	__yield_interrupt bool try_pump_msg(const msg_pump_handle<>& pump);

	template <typename... Args, typename Handler>
	__yield_interrupt bool try_pump_msg_invoke(bool checkDis, const msg_pump_handle<Args...>& pump, Handler&& h)
	{
		assert_enter();
		DstReceiverBuff_<Args...> dstRec;
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			tuple_invoke(h, std::move(dstRec._dstBuff.get()));
			return true;
		}
		return false;
	}

	template <typename... Args, typename Handler>
	__yield_interrupt bool try_pump_msg_invoke(const msg_pump_handle<Args...>& pump, Handler&& h)
	{
		return try_pump_msg_invoke(false, pump, h);
	}

	/*!
	@brief 从消息泵中提取消息
	*/
	template <typename... Args, typename... Outs>
	__yield_interrupt void pump_msg(bool checkDis, const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		timed_pump_msg(-1, checkDis, pump, res...);
	}

	template <typename... Args, typename... Outs>
	__yield_interrupt void pump_msg(const msg_pump_handle<Args...>& pump, Outs&... res)
	{
		pump_msg(false, pump, res...);
	}

	template <typename R>
	__yield_interrupt R pump_msg(bool checkDis, const msg_pump_handle<R>& pump)
	{
		assert_enter();
		DstReceiverBuff_<R> dstRec;
		_pump_msg(pump, dstRec, checkDis);
		return std::move(std::get<0>(dstRec._dstBuff.get()));
	}

	template <typename R>
	__yield_interrupt R pump_msg(const msg_pump_handle<R>& pump)
	{
		return pump_msg(false, pump);
	}

	__yield_interrupt void pump_msg(bool checkDis, const msg_pump_handle<>& pump);

	__yield_interrupt void pump_msg(const msg_pump_handle<>& pump);

	template <typename... Args, typename Handler>
	__yield_interrupt void pump_msg_invoke(bool checkDis, const msg_pump_handle<Args...>& pump, Handler&& h)
	{
		timed_pump_msg_invoke<Args...>(-1, checkDis, pump, h);
	}

	template <typename... Args, typename Handler>
	__yield_interrupt void pump_msg_invoke(const msg_pump_handle<Args...>& pump, Handler&& h)
	{
		pump_msg_invoke(false, pump, h);
	}

	/*!
	@brief 获取当前消息准确数
	*/
	template <typename... Args>
	__yield_interrupt size_t pump_length(const msg_pump_handle<Args...>& pump)
	{
		assert(!pump.check_closed());
		return pump._handle->size();
	}

	/*!
	@brief 获取当前消息大概数
	*/
	template <typename... Args>
	size_t pump_snap_length(const msg_pump_handle<Args...>& pump)
	{
		assert(!pump.check_closed());
		return pump._handle->snap_size();
	}

	/*!
	@brief 超时等待通知句柄连接
	*/
	template <typename... Args>
	__yield_interrupt bool timed_wait_connect(int ms, msg_pump_handle<Args...>& pump)
	{
		return _timed_wait_connect(pump, ms);
	}

	/*!
	@brief 等待通知句柄连接
	*/
	template <typename... Args>
	__yield_interrupt void wait_connect(msg_pump_handle<Args...>& pump)
	{
		_timed_wait_connect(pump, -1);
	}

	/*!
	@brief 在一定时间内尝试弹出并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt bool timed_pump_ignore_msg(int ms, bool checkDis, const msg_pump_handle<Args...>& pump)
	{
		std::tuple<ignore_msg<Args>...> ignoreMsg;
		return tuple_invoke<bool>(&my_actor::_timed_pump_ignore_msg<Args...>, std::tuple<my_actor*, int&, bool&, const msg_pump_handle<Args...>&>(this, ms, checkDis, pump), ignoreMsg);
	}

	template <typename... Args>
	__yield_interrupt bool timed_pump_ignore_msg(int ms, const msg_pump_handle<Args...>& pump)
	{
		return timed_pump_ignore_msg(ms, false, pump);
	}

	/*!
	@brief 尝试弹出并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt bool try_pump_ignore_msg(bool checkDis, const msg_pump_handle<Args...>& pump)
	{
		std::tuple<ignore_msg<Args>...> ignoreMsg;
		return tuple_invoke<bool>(&my_actor::_try_pump_ignore_msg<Args...>, std::tuple<my_actor*, bool&, const msg_pump_handle<Args...>&>(this, checkDis, pump), ignoreMsg);
	}

	template <typename... Args>
	__yield_interrupt bool try_pump_ignore_msg(const msg_pump_handle<Args...>& pump)
	{
		return try_pump_ignore_msg(false, pump);
	}

	/*!
	@brief 等待并忽略掉一个消息
	*/
	template <typename... Args>
	__yield_interrupt void pump_ignore_msg(bool checkDis, const msg_pump_handle<Args...>& pump)
	{
		timed_pump_ignore_msg(-1, checkDis, pump);
	}

	template <typename... Args>
	__yield_interrupt void pump_ignore_msg(const msg_pump_handle<Args...>& pump)
	{
		pump_ignore_msg(false, pump);
	}
private:
	template <typename... Args>
	__yield_interrupt static bool _timed_pump_ignore_msg(my_actor* const host, int ms, bool checkDis, const msg_pump_handle<Args...>& pump, ignore_msg<Args>&... res)
	{
		return host->timed_pump_msg(ms, checkDis, pump, res...);
	}

	template <typename... Args>
	__yield_interrupt static bool _try_pump_ignore_msg(my_actor* const host, bool checkDis, const msg_pump_handle<Args...>& pump, ignore_msg<Args>&... res)
	{
		return host->try_pump_msg(checkDis, pump, res...);
	}
public:
	/*!
	@brief 查询当前消息由谁代理
	*/
	template <typename... Args>
	__yield_interrupt actor_handle msg_agent_handle(const int id, const actor_handle& buddyActor)
	{
		typedef std::shared_ptr<msg_pool_status::pck<Args...>> pck_type;

		lock_suspend();
		lock_quit();
		auto msgPck = send<pck_type>(buddyActor->self_strand(), [id, &buddyActor]()->pck_type
		{
			if (!buddyActor->_quited)
			{
				return my_actor::msg_pool_pck<Args...>(id, buddyActor.get(), false);
			}
			return pck_type();
		});
		if (msgPck)
		{
			msgPck->lock(this);
			size_t stackl = 0;
			msg_pool_status::pck<Args...>* uStack[16];
			msg_pool_status::pck<Args...>* pckIt = msgPck.get();
			while (true)
			{
				if (pckIt->_next)
				{
					assert(stackl < 15);
					pckIt = pckIt->_next.get();
					uStack[stackl++] = pckIt;
					pckIt->lock(this);
				}
				else
				{
					actor_handle r;
					if (!pckIt->is_closed())
					{
						r = pckIt->_hostActor->shared_from_this();
					}
					while (stackl)
					{
						uStack[--stackl]->unlock(this);
					}
					msgPck->unlock(this);
					unlock_quit();
					unlock_suspend();
					return r;
				}
			}
		}
		unlock_quit();
		unlock_suspend();
		return actor_handle();
	}

	template <typename... Args>
	__yield_interrupt actor_handle msg_agent_handle(const int id, child_handle& buddyActor)
	{
		return msg_agent_handle<Args...>(id, buddyActor.get_actor());
	}

	template <typename... Args>
	__yield_interrupt actor_handle msg_agent_handle(const actor_handle& buddyActor)
	{
		return msg_agent_handle<Args...>(0, buddyActor);
	}

	template <typename... Args>
	__yield_interrupt actor_handle msg_agent_handle(child_handle& buddyActor)
	{
		return msg_agent_handle<Args...>(0, buddyActor.get_actor());
	}

	template <typename... Args>
	__yield_interrupt actor_handle msg_agent_handle(const int id = 0)
	{
		return msg_agent_handle<Args...>(id, shared_from_this());
	}
private:
	static bool _select_ready(MutexBlock_** const mbs, const size_t N)
	{
		bool r = false;
		for (size_t i = 0; i < N; i++)
		{
			r |= mbs[i]->ready();
		}
		return r;
	}

	static bool _select_ready2(const size_t st, MutexBlock_** const mbs, const size_t N)
	{
		assert(st < N);
		size_t i = st;
		while (!mbs[i]->ready())
		{
			i = (i + 1) % N;
			if (st == i)
			{
				return false;
			}
		}
		return true;
	}

	static void _select_cancel(MutexBlock_** const mbs, const size_t N)
	{
		for (size_t i = 0; i < N; i++)
		{
			mbs[i]->cancel();
		}
	}

	static bool _select_go(MutexBlock_** const mbs, const size_t N)
	{
		try
		{
			bool isQuit = false;
			for (size_t i = 0; i < N; i++)
			{
				bool isRun = false;
				isQuit |= mbs[i]->go_run(isRun);
			}
			return isQuit;
		}
		catch (select_msg_block_force_quit&)
		{
			return true;
		}
		DEBUG_OPERATION(catch (...) { assert(false); } return true;)
	}

	static void _select_check_lost(MutexBlock_** const mbs, const size_t N)
	{
#ifdef ENABLE_CHECK_LOST
		for (size_t i = 0; i < N; i++)
		{
			mbs[i]->check_lost();
		}
#endif
	}

	static bool _select_go_count(size_t& runCount, MutexBlock_** const mbs, const size_t N)
	{
		try
		{
			bool isQuit = false;
			for (size_t i = 0; i < N; i++)
			{
				bool isRun = false;
				isQuit |= mbs[i]->go_run(isRun);
				if (isRun)
				{
					runCount++;
				}
			}
			return isQuit;
		}
		catch (select_msg_block_force_quit&)
		{
			runCount++;
			return true;
		}
		DEBUG_OPERATION(catch (...) { assert(false); } return true;)
	}

	static bool _cmp_snap_id(MutexBlock_** const mbs, const size_t N)
	{
		for (size_t i = 0; i < N - 1; i++)
		{
			size_t t = mbs[i]->snap_id();
			for (size_t j = i + 1; j < N; j++)
			{
				if (mbs[j]->snap_id() == t)
				{
					return false;
				}
			}
		}
		return true;
	}

	static void _check_host_id(my_actor* self, MutexBlock_** const mbs, const size_t N)
	{
		for (size_t i = 0; i < N; i++)
		{
			assert(mbs[i]->host_id() == self->self_id());
		}
	}

	template <typename Ready>
	__yield_interrupt void _select_msg_blocks(Ready&& mutexReady, MutexBlock_** const mbList, const size_t N)
	{
		lock_quit();
		DEBUG_OPERATION(_check_host_id(this, mbList, N));//判断句柄是不是都是自己的
		assert(_cmp_snap_id(mbList, N));//判断有没有重复参数
		BREAK_OF_SCOPE_EXEC(_select_cancel(mbList, N));
		do
		{
			DEBUG_OPERATION(auto nt = yield_count());
			if (!mutexReady())
			{
				assert(yield_count() == nt);
				unlock_quit();
				_select_check_lost(mbList, N);
				push_yield();
				_select_check_lost(mbList, N);
				lock_quit();
				DEBUG_OPERATION(nt = yield_count());
			}
			_select_cancel(mbList, N);
			assert(yield_count() == nt);
		} while (!_select_go(mbList, N));
		unlock_quit();
	}

	template <typename Ready>
	__yield_interrupt size_t _timed_select_msg_blocks(const int ms, Ready&& mutexReady, MutexBlock_** const mbList, const size_t N)
	{
		lock_quit();
		size_t runCount = 0;
		DEBUG_OPERATION(_check_host_id(this, mbList, N));//判断句柄是不是都是自己的
		assert(_cmp_snap_id(mbList, N));//判断有没有重复参数
		BREAK_OF_SCOPE_EXEC(_select_cancel(mbList, N));
		do
		{
			DEBUG_OPERATION(auto nt = yield_count());
			if (!mutexReady())
			{
				assert(yield_count() == nt);
				unlock_quit();
				_select_check_lost(mbList, N);
				if (ms >= 0)
				{
					bool overtime = false;
					delay_trig(ms, [this, &overtime]
					{
						overtime = true;
						pull_yield();
					});
					push_yield();
					if (overtime)
					{
						lock_quit();
						break;
					}
					cancel_delay_trig();
				}
				else
				{
					push_yield();
				}
				_select_check_lost(mbList, N);
				lock_quit();
				DEBUG_OPERATION(nt = yield_count());
			}
			_select_cancel(mbList, N);
			assert(yield_count() == nt);
		} while (!_select_go_count(runCount, mbList, N));
		unlock_quit();
		return runCount;
	}
public:
	/*!
	@brief 运行select消息执行块（阻塞），每次有几个取几个
	*/
	template <typename... MutexBlocks>
	__yield_interrupt void select_msg_blocks_many(MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		_select_msg_blocks([&]()->bool
		{
			return _select_ready(mbList, N);
		}, mbList, N);
	}

	/*!
	@brief 运行select消息执行块（阻塞），每次从头开始优先只取一条消息
	*/
	template <typename... MutexBlocks>
	__yield_interrupt void select_msg_blocks_safe(MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		_select_msg_blocks([&]()->bool
		{
			return _select_ready2(0, mbList, N);
		}, mbList, N);
	}

	/*!
	@brief 运行select消息执行块（阻塞），每次依次往后开始优先只取一条消息
	*/
	template <typename... MutexBlocks>
	__yield_interrupt void select_msg_blocks_rotation(MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		size_t i = -1;
		_select_msg_blocks([&]()->bool
		{
			i = N - 1 != i ? i + 1 : 0;
			return _select_ready2(i, mbList, N);
		}, mbList, N);
	}

	/*!
	@brief 运行select消息执行块（阻塞，带优先级），每次只取一条消息
	*/
	template <typename... MutexBlocks>
	__yield_interrupt void select_msg_blocks_priority(MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		const size_t m = 2 * N + 1;
		const size_t cmax = N* (N + 1) / 2;
		size_t ct = 0;
		_select_msg_blocks([&]()->bool
		{
			ct = cmax != ct ? ct + 1 : 1;
			const size_t i = (m + 1 - (size_t)std::sqrt(m * m - 8 * ct)) / 2 - 1;
			return _select_ready2(i, mbList, N);
		}, mbList, N);
	}

	/*!
	@brief 超时等待运行select消息执行块（阻塞），每次有几个取几个
	*/
	template <typename... MutexBlocks>
	__yield_interrupt size_t timed_select_msg_blocks_many(const int ms, MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		return _timed_select_msg_blocks(ms, [&]()->bool
		{
			return _select_ready(mbList, N);
		}, mbList, N);
	}

	/*!
	@brief 超时等待运行select消息执行块（阻塞），每次从头开始优先只取一条消息
	*/
	template <typename... MutexBlocks>
	__yield_interrupt size_t timed_select_msg_blocks_safe(const int ms, MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		return _timed_select_msg_blocks(ms, [&]()->bool
		{
			return _select_ready2(0, mbList, N);
		}, mbList, N);
	}

	/*!
	@brief 超时等待运行select消息执行块（阻塞），每次依次往后开始优先只取一条消息
	*/
	template <typename... MutexBlocks>
	__yield_interrupt size_t timed_select_msg_blocks_rotation(const int ms, MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		size_t i = -1;
		return _timed_select_msg_blocks(ms, [&]()->bool
		{
			i = N - 1 != i ? i + 1 : 0;
			return _select_ready2(i, mbList, N);
		}, mbList, N);
	}

	/*!
	@brief 超时等待运行select消息执行块（阻塞，带优先级），每次只取一条消息
	*/
	template <typename... MutexBlocks>
	__yield_interrupt size_t timed_select_msg_blocks_priority(const int ms, MutexBlocks&&... mbs)
	{
		const size_t N = sizeof...(MutexBlocks);
		static_assert(N > 0, "");
		MutexBlock_* mbList[N] = { &mbs... };
		const size_t m = 2 * N + 1;
		const size_t cmax = N* (N + 1) / 2;
		size_t ct = 0;
		return _timed_select_msg_blocks(ms, [&]()->bool
		{
			ct = cmax != ct ? ct + 1 : 1;
			const size_t i = (m + 1 - (size_t)std::sqrt(m * m - 8 * ct)) / 2 - 1;
			return _select_ready2(i, mbList, N);
		}, mbList, N);
	}
public:
	/*!
	@brief 获取当前代码运行在哪个Actor下
	*/
	static my_actor* self_actor();

	/*!
	@brief 检测当前是否在该Actor内运行
	*/
	void check_self();

	/*!
	@brief 测试当前下的Actor栈是否安全
	*/
	void check_stack();

	/*!
	@brief 设置actor局部存储
	*/
	void als_set(void* val);

	/*!
	@brief 获取actor局部存储
	*/
	void* als_get();

	/*!
	@brief 获取当前Actor剩余安全栈空间
	*/
	size_t stack_idle_space();

	/*!
	@brief 获取当前Actor调度器
	*/
	const shared_strand& self_strand();

	/*!
	@brief 获取当前调度器定时器
	*/
	overlap_timer* over_timer();

	/*!
	@brief 获取io_service调度器
	*/
	boost::asio::io_service& self_io_service();

	/*!
	@brief 获取io_engine调度器
	*/
	io_engine& self_io_engine();

	/*!
	@brief 返回本对象的智能指针
	*/
	actor_handle shared_from_this();

	/*!
	@brief 创建一个异步定时器
	*/
	async_timer make_timer();

	/*!
	@brief 获取当前ActorID号
	*/
	id self_id();

	/*!
	@brief 当前Actor执行体预设的key
	*/
	size_t self_key();

	/*!
	@brief 当前Actor使用的reusable_mem
	*/
	reusable_mem& self_reusable();

	/*!
	@brief 设置退出码
	*/
	void return_code(size_t cd);

	/*!
	@brief 获取退出码
	*/
	size_t return_code();

	/*!
	@brief Actor退出后，获取栈消耗大小
	*/
	size_t using_stack_size();

	/*!
	@brief Actor栈大小
	*/
	size_t stack_size();

	/*!
	@brief Actor栈总大小
	*/
	size_t stack_total_size();

	/*!
	@brief 获取Actor切换计数
	*/
	size_t yield_count();

	/*!
	@brief Actor切换计数清零
	*/
	void reset_yield();

	/*!
	@brief 开始运行建立好的Actor
	*/
	void run();

	/*!
	@brief 强制退出该Actor，不可滥用，有可能会造成资源泄漏，完成后回调
	*/
	void force_quit(std::function<void()> h = std::function<void()>());

	/*!
	@brief Actor是否已经开始运行
	*/
	bool is_started();

	/*!
	@brief Actor是否已经退出
	*/
	bool is_exited();

	/*!
	@brief 是否是强制退出（在Actor确认退出后调用）
	*/
	bool is_force();

	/*!
	@brief 是否在Actor中
	*/
	bool in_actor();

	/*!
	@brief 是否在该Actor依赖的同一个strand中运行
	*/
	bool running_in_actor_strand();

	/*!
	@brief lock_quit后检测是否收到退出消息
	*/
	bool quit_msg();

	/*!
	@brief 锁定当前Actor，暂时不让强制退出
	*/
	void lock_quit();

	/*!
	@brief 解除退出锁定
	*/
	void unlock_quit();

	/*!
	@brief 锁定当前Actor，暂时不让挂起
	*/
	void lock_suspend();

	/*!
	@brief 解除挂起锁定
	*/
	void unlock_suspend();

	/*!
	@brief 是否锁定了退出
	*/
	bool is_locked_quit();

	/*!
	@brief 是否锁定了挂起
	*/
	bool is_locked_suspend();

	/*!
	@brief Actor结束后释放栈内存
	*/
	void after_exit_clean_stack();

	/*!
	@brief 暂停Actor
	*/
	void suspend(std::function<void()> h = std::function<void()>());

	/*!
	@brief 恢复已暂停Actor
	*/
	void resume(std::function<void()> h = std::function<void()>());

	/*!
	@brief 触发通知，0 <= id < 32,64
	*/
	void notify_trig_sign(int id);

	/*!
	@brief 重置触发标记
	*/
	void reset_trig_sign(int id);

	/*!
	@brief 切换挂起/非挂起状态
	*/
	void switch_pause_play(std::function<void(bool)> h = std::function<void(bool)>());

	/*!
	@brief 等待Actor退出，在Actor所依赖的ios无关线程中使用
	*/
	void outside_wait_quit();

	/*!
	@brief 添加一个Actor结束通知
	*/
	void append_quit_notify(std::function<void()> h = std::function<void()>());

	/*!
	@brief 添加一个Actor结束时在strand中执行的函数，后添加的先执行
	*/
	void append_quit_executor(std::function<void()> h = std::function<void()>());

	/*!
	@brief 启动一堆Actor
	*/
	void actors_start_run(const std::list<actor_handle>& anotherActors);

	template <typename Alloc>
	void actors_start_run(const std::list<actor_handle, Alloc>& anotherActors)
	{
		assert_enter();
		for (auto& actorHandle : anotherActors)
		{
			actorHandle->run();
		}
	}

	/*!
	@brief 强制退出另一个Actor，并且等待完成
	*/
	__yield_interrupt void actor_force_quit(const actor_handle& anotherActor);

	/*!
	@brief 强制退出一堆Actor，并且等待完成
	*/
	__yield_interrupt void actors_force_quit(const std::list<actor_handle>& anotherActors);

	template <typename Alloc>
	__yield_interrupt void actors_force_quit(const std::list<actor_handle, Alloc>& anotherActors)
	{
		assert_enter();
		msg_handle<> amh;
		msg_notifer<> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : anotherActors)
		{
			actorHandle->force_quit(h);
		}
		for (size_t i = anotherActors.size(); i > 0; i--)
		{
			wait_msg(amh);
		}
		close_msg_notifer(amh);
	}

	/*!
	@brief 等待另一个Actor结束后返回
	*/
	__yield_interrupt void actor_wait_quit(const actor_handle& anotherActor);

	/*!
	@brief 等待一堆Actor结束后返回
	*/
	__yield_interrupt void actors_wait_quit(const std::list<actor_handle>& anotherActors);

	template <typename Alloc>
	__yield_interrupt void actors_wait_quit(const std::list<actor_handle, Alloc>& anotherActors)
	{
		assert_enter();
		for (auto& actorHandle : anotherActors)
		{
			actor_wait_quit(actorHandle);
		}
	}

	__yield_interrupt bool timed_actor_wait_quit(int ms, const actor_handle& anotherActor);

	/*!
	@brief 挂起另一个Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actor_suspend(const actor_handle& anotherActor);

	/*!
	@brief 挂起一堆Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actors_suspend(const std::list<actor_handle>& anotherActors);

	template <typename Alloc>
	__yield_interrupt void actors_suspend(const std::list<actor_handle, Alloc>& anotherActors)
	{
		assert_enter();
		lock_quit();
		msg_handle<> amh;
		msg_notifer<> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : anotherActors)
		{
			actorHandle->suspend(wrap_ref_handler(h));
		}
		for (size_t i = anotherActors.size(); i > 0; i--)
		{
			wait_msg(amh);
		}
		close_msg_notifer(amh);
		unlock_quit();
	}

	/*!
	@brief 恢复另一个Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actor_resume(const actor_handle& anotherActor);

	/*!
	@brief 恢复一堆Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actors_resume(const std::list<actor_handle>& anotherActors);

	template <typename Alloc>
	__yield_interrupt void actors_resume(const std::list<actor_handle, Alloc>& anotherActors)
	{
		assert_enter();
		lock_quit();
		msg_handle<> amh;
		msg_notifer<> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : anotherActors)
		{
			actorHandle->resume(wrap_ref_handler(h));
		}
		for (size_t i = anotherActors.size(); i > 0; i--)
		{
			wait_msg(amh);
		}
		close_msg_notifer(amh);
		unlock_quit();
	}

	/*!
	@brief 对另一个Actor进行挂起/恢复状态切换
	@return 都已挂起返回true，否则false
	*/
	__yield_interrupt bool actor_switch(const actor_handle& anotherActor);

	/*!
	@brief 对一堆Actor进行挂起/恢复状态切换
	@return 都已挂起返回true，否则false
	*/
	__yield_interrupt bool actors_switch(const std::list<actor_handle>& anotherActors);

	template <typename Alloc>
	__yield_interrupt bool actors_switch(const std::list<actor_handle, Alloc>& anotherActors)
	{
		assert_enter();
		lock_quit();
		bool isPause = true;
		msg_handle<bool> amh;
		msg_notifer<bool> h = make_msg_notifer_to_self(amh);
		for (auto& actorHandle : anotherActors)
		{
			actorHandle->switch_pause_play(wrap_ref_handler(h));
		}
		for (size_t i = anotherActors.size(); i > 0; i--)
		{
			isPause &= wait_msg(amh);
		}
		close_msg_notifer(amh);
		unlock_quit();
		return isPause;
	}

	void assert_enter();

	/*!
	@brief 安装my_actor框架
	*/
	static void install();
	static void install(const shared_initer*);
	static const shared_initer* get_initer();

	/*!
	@brief 卸载my_actor框架
	*/
	static void uninstall();

	/*!
	@brief 
	*/
	static void tls_init();
	static void tls_uninit();
private:
	template <typename Handler>
	void timeout(int ms, Handler&& handler)
	{
		assert(ms >= 0);
		assert(_timerStateCompleted);
		assert(!_timerStateCb);
		typedef wrap_timer_handler<Handler> wrap_type;
		_timerStateCompleted = false;
		_timerStateTime = (long long)ms * 1000;
		_timerStateCb = new(_reuMem.allocate(sizeof(wrap_type)))wrap_type(handler);
		_timerStateHandle = _strand->actor_timer()->timeout(_timerStateTime, shared_from_this());
	}

	template <typename Handler>
	void deadline(long long us, Handler&& handler)
	{
		assert(_timerStateCompleted);
		assert(!_timerStateCb);
		typedef wrap_timer_handler<Handler> wrap_type;
		_timerStateCompleted = false;
		_timerStateCb = new(_reuMem.allocate(sizeof(wrap_type)))wrap_type(handler);
		_timerStateHandle = _strand->actor_timer()->timeout(us, shared_from_this(), true);
		_timerStateTime = us > _timerStateHandle._beginStamp ? us - _timerStateHandle._beginStamp : 0;
	}

	void timeout_handler();
	void cancel_timer();
	void suspend_timer();
	void resume_timer();
	void begin_suspend();
	void begin_resume();
	void child_suspend_then();
	void child_resume_then();
	void run_one();
	void pull_yield_tls();
	void pull_yield();
	void pull_yield_after_quited();
	void push_yield();
	void push_yield_after_quited();
#if (__linux__ && ENABLE_DUMP_STACK)
	static void dump_segmentation_fault(void* sp, size_t length);
	static void undump_segmentation_fault();
#endif
public:
#ifdef PRINT_ACTOR_STACK
	std::list<stack_line_info> _createStack;///<当前Actor创建时的调用堆栈
#endif
private:
	std::weak_ptr<my_actor> _weakThis;
	shared_strand _strand;///<Actor调度器
	id _selfID;///<ActorID
	void* _alsVal;///<actor局部存储
	actor_pull_type* _actorPull;///<Actor中断点恢复
	actor_push_type* _actorPush;///<Actor中断点
	wrap_timer_handler_face* _timerStateCb;///<定时器触发回调
	size_t _actorKey;///<该Actor处理模块的全局唯一key
	size_t _lockQuit;///<锁定当前Actor，如果当前接收到退出消息，暂时不退，等到解锁后退出
	size_t _lockSuspend;///锁定当前Actor的挂起操作，如果当前接收到挂起消息，暂时不挂起，等到解锁后挂起
	size_t _yieldCount;///<yield计数
	size_t _lastYield;///<记录上次try_yield的计数
	size_t _childOverCount;///<子Actor退出时计数
	size_t _childSuspendResumeCount;///<子Actor挂起/恢复计数
	size_t _returnCode;///<退出码
	size_t _usingStackSize;///<栈消耗
	size_t _trigSignMask;///<触发消息标记
	size_t _waitingTrigMask;///<等待触发消息标记
	long long _timerStateTime;///<当前定时时间
	long long _timerStateStampEnd;///<定时结束时间
	msg_pool_status _msgPoolStatus;///<消息池列表
	actor_handle _parentActor;///<父Actor，子Actor都析构后，父Actor才能析构
	ActorTimer_::timer_handle _timerStateHandle;///<定时器句柄
	reusable_mem _reuMem;///<定时器内存管理
	main_func _mainFunc;///<Actor入口
	std::list<suspend_resume_option> _suspendResumeQueue;///<挂起/恢复操作队列
	std::list<std::function<void()> > _quitCallback;///<Actor结束后的回调函数
	std::list<std::function<void()> > _beginQuitExec;///<Actor准备退出时调用的函数，后注册的先执行
	std::list<actor_handle> _childActorList;///<子Actor集合，子Actor都退出后，父Actor才能退出
	int _timerStateCount;///<定时器计数
	bool _timerStateSuspend : 1;///<定时器是否挂起
	bool _timerStateCompleted : 1;///<定时器是否完成
	bool _inActor : 1;///<当前正在Actor内部执行标记
	bool _started : 1;///<已经开始运行的标记
	bool _quited : 1;///<_mainFunc已经不再执行
	bool _exited : 1;///<完全退出
	bool _suspended : 1;///<Actor挂起标记
	bool _isForce : 1;///<是否是强制退出的标记，成功调用了force_quit
	bool _holdPull : 1;///<当前Actor挂起，有外部触发准备进入Actor标记
	bool _holdQuited : 1;///<当前Actor被锁定后，收到退出消息
	bool _holdSuspended : 1;///<挂起恢复操作没挂起标记
	bool _checkStack : 1;///<是否检测栈空间
	bool _waitingQuit : 1;///<等待退出标记
	bool _afterExitCleanStack : 1;///<结束后清栈
#ifdef PRINT_ACTOR_STACK
public:
	bool _checkStackFree : 1;///<是否检测堆栈过多
private:
#endif
	static std::atomic<my_actor::id>* _actorIDCount;///<ID计数
};

//////////////////////////////////////////////////////////////////////////

struct ActorReadyGo_
{
	ActorReadyGo_(shared_strand strand, size_t stackSize = MAX_STACKSIZE);
	ActorReadyGo_(io_engine& ios, size_t stackSize = MAX_STACKSIZE);
	ActorReadyGo_(shared_strand strand, std::function<void()> notify, size_t stackSize = MAX_STACKSIZE);
	ActorReadyGo_(io_engine& ios, std::function<void()> notify, size_t stackSize = MAX_STACKSIZE);

	template <typename Handler>
	actor_handle operator -(AutoStackActor_<Handler>&& wrapActor)
	{
		assert(_strand);
		if (_notify)
		{
			return my_actor::create_and_notify(std::move(_strand), (AutoStackActorFace_&&)wrapActor, std::move(_notify));
		}
		return my_actor::create(std::move(_strand), std::move(wrapActor));
	}

	template <typename Handler>
	actor_handle operator -(Handler&& handler)
	{
		assert(_strand);
		if (_notify)
		{
			return my_actor::create_and_notify(std::move(_strand), std::move(handler), std::move(_notify), _stackSize);
		}
		return my_actor::create(std::move(_strand), std::forward<Handler>(handler), _stackSize);
	}

	shared_strand _strand;
	std::function<void()> _notify;
	size_t _stackSize;
	NONE_COPY(ActorReadyGo_);
};

struct ActorGo_ : protected ActorReadyGo_
{
	template <typename... Args>
	ActorGo_(Args&&... args)
		:ActorReadyGo_(std::forward<Args>(args)...) {}

	template <typename Handler>
	actor_handle operator -(AutoStackActor_<Handler>&& wrapActor)
	{
		actor_handle actor = ActorReadyGo_::operator-(std::move(wrapActor));
		actor->run();
		return actor;
	}

	template <typename Handler>
	actor_handle operator -(Handler&& handler)
	{
		actor_handle actor = ActorReadyGo_::operator-(std::forward<Handler>(handler));
		actor->run();
		return actor;
	}

	NONE_COPY(ActorGo_);
};

#define go(...) ActorGo_(__VA_ARGS__)-
#define ready_go(...) ActorReadyGo_(__VA_ARGS__)-

//////////////////////////////////////////////////////////////////////////

template <typename R, typename H>
R ActorFunc_::send(my_actor* host, const shared_strand& exeStrand, H&& h)
{
	assert(host);
	return host->send<R>(exeStrand, std::forward<H>(h));
}

template <typename R, typename H>
R ActorFunc_::async_send(my_actor* host, const shared_strand& exeStrand, H&& h)
{
	assert(host);
	return host->async_send<R>(exeStrand, std::forward<H>(h));
}

template <typename... Args>
msg_pump_handle<Args...> ActorFunc_::connect_msg_pump(const int id, my_actor* const host, bool checkLost)
{
	assert(host);
	return my_actor::_connect_msg_pump<Args...>(id, host, checkLost);
}

template <typename H>
void ActorFunc_::delay_trig(my_actor* host, int ms, H&& h)
{
	assert(host);
	host->delay_trig(ms, std::forward<H>(h));
}

template <typename DST, typename SRC>
void ActorFunc_::_trig_handler(my_actor* host, bool* sign, DST& dstRec, SRC&& args)
{
	assert(host);
	host->_trig_handler(sign, dstRec, std::forward<SRC>(args));
}

template <typename DST, typename SRC>
void ActorFunc_::_trig_handler2(my_actor* host, shared_bool& closed, bool* sign, DST& dstRec, SRC&& args)
{
	assert(host);
	host->_trig_handler2(closed, sign, dstRec, std::forward<SRC>(args));
}

#endif