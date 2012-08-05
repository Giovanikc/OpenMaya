#ifndef MTAP_QUEUE_H
#define MTAP_QUEUE_H

#include <queue>
#include "boost/thread/mutex.hpp"
#include "boost/thread/condition_variable.hpp"

namespace EventQueue 
{

struct Event
{
	enum Types{
		INTERRUPT = 0,
		TILEDONE = 1,
		RENDERDONE = 2,
		PRETILE = 3,
		FRAMEUPDATE = 4,
		IPRSTART = 5,
		IPRSTOP = 6,
		IPRPAUSE = 7,
		IPRREMOVE = 8,
		IPRUPDATE = 9,
		USER = 10
	};
	Types type;
	void *data;
	size_t tile_xmin, tile_xmax, tile_ymin, tile_ymax;
	Event()
	{
		type = INTERRUPT;
		data = NULL;
	}
};


template<typename Data>
class concurrent_queue
{
private:
    std::queue<Data> the_queue;
    mutable boost::mutex the_mutex;
    boost::condition_variable the_condition_variable;
public:
    void push(Data const& data)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        the_queue.push(data);
        lock.unlock();
        the_condition_variable.notify_one();
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(the_mutex);
        return the_queue.empty();
    }

    bool try_pop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        if(the_queue.empty())
        {
            return false;
        }
        
        popped_value=the_queue.front();
        the_queue.pop();
        return true;
    }

    void wait_and_pop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        while(the_queue.empty())
        {
            the_condition_variable.wait(lock);
        }
        popped_value=the_queue.front();
        the_queue.pop();
    }

};


static concurrent_queue<Event> EventList;

concurrent_queue<Event> *theQueue();

} // namespace EventQueue

#endif