#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

using std::mutex;
using std::unique_lock;
using std::future;
using std::result_of;
using std::thread;
using std::forward;

class threadpool {
	public:
		// Starts thread pool
		threadpool(int thread_size) : stop(false) {
			for(int i = 0; i<thread_size; ++i) {
				// add generic lambda func to pool
				threads.emplace_back( [this] {
					while(true) {
						std::function<void()> job;
						{
							unique_lock<mutex> lock(this->q_mutex);
							this->cond.wait(lock,
								[this]{ return this->stop || !this->jobs.empty(); });
							if(this->stop && this->jobs.empty())
								return;
							job = std::move(this->jobs.front());
							this->jobs.pop();
						}
						job();
					}
				});
			}	  
		}

		template<class TP, class... Arguments>
		auto work(TP&& f, Arguments&&... args) 
			-> future<typename result_of<TP(Arguments...)>::type>;

		//deconstructor
		~threadpool(){			
			unique_lock<mutex> lock(q_mutex);
			stop = true;
		
			cond.notify_all();
			for(thread &t: threads) {
				t.join();
			}
		}

	private:
		std::vector<thread> threads;
		std::queue<std::function<void()>> jobs;

		mutex q_mutex;
		std::condition_variable cond;
		bool stop;
	};
 
// templated function that allows thread pool to be genericly used by any
// calling function
template<class TP, class... Arguments>
auto threadpool::work(TP&& t, Arguments&&... args) 
	-> future<typename result_of<TP(Arguments...)>::type> {
	using return_type = typename result_of<TP(Arguments...)>::type;

	auto job = std::make_shared< std::packaged_task<return_type()> >(
			std::bind(forward<TP>(t), forward<Arguments>(args)...));
        
	future<return_type> result = job->get_future();
	{
        unique_lock<std::mutex> lock(q_mutex);

		if(stop) {
			exit(1);
		}

		jobs.emplace([job](){ (*job)(); });
	}
	cond.notify_one();
	return result;
}

