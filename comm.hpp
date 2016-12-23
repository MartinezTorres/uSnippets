////////////////////////////////////////////////////////////////////////
// communication library
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once
#include <uSnippets/log.hpp>
#include <uSnippets/time.hpp>
#include <uSnippets/object.hpp>
#define ASIO_STANDALONE
#include <boost/asio.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/regex.hpp>

#include <iostream>

#include <list>
#include <map>
#include <unordered_map>

#include <opencv/highgui.h>

namespace uSnippets {
namespace comm {
	
	
	namespace Net { class Connection; }

	class Message {

		friend Net::Connection;
		static const uint32_t magic = 0x50E8E1E8UL;

		struct Header {
			uint64_t tsStart, tsEnd; //in us, will overflow year 586912 approx 
			int8_t priority;
			char ID[15];
			uint32_t size = 0;
			uint32_t magic = Message::magic;
		}; // 40 bytes

		Header header;
		std::shared_ptr<std::string> data;

	public:
		explicit Message( const std::string &ID, std::shared_ptr<std::string> data) : data(data) {
			
			this->ts(now());
			this->ID(ID);
			this->priority(0);
		}
	
		explicit Message( const std::string &ID="" ) : Message(ID, std::make_shared<std::string>()){}

		explicit Message( const std::string &ID, const std::string &data ) : Message(ID, std::make_shared<std::string>(data)){}

		explicit Message( const std::string &ID, const char *data ) : Message(ID, std::make_shared<std::string>(data)){}

		template<typename T=void> 
		explicit Message( const std::string &ID, const T *data, int size ) : Message(ID, std::make_shared<std::string>((const char *)data, size)){}

		bool operator==(const Message& rhs) const { 
			return header.tsStart == rhs.header.tsStart 
			   and header.tsEnd == rhs.header.tsEnd
			   and ID() == rhs.ID()
			   and data.get() == rhs.data.get();
		}
	 
		template<class Archive>
		void serialize(Archive & ar, const unsigned int) const {
			ar & header.tsStart & header.tsEnd & header.priority & header.ID & header.size & header.magic & (std::string &)(*data);
		}

		template<class Archive>
		void serialize(Archive & ar, const unsigned int) {
			ar & header.tsStart & header.tsEnd & header.priority & header.ID & header.size & header.magic & (std::string &)(*data);
		}

		Message &ts( time_point start, time_point end = time_point(0_s) ) { 
			header.tsStart = std::chrono::duration_cast<microseconds>(start.time_since_epoch()).count(); 
			header.tsEnd = std::chrono::duration_cast<microseconds>((end>start?end:start).time_since_epoch()).count(); 
			return *this;
		}
		
		const time_point tsStart() const { return time_point(microseconds(header.tsStart)); }
		const time_point tsEnd()   const { return time_point(microseconds(header.tsEnd)); }

		void priority(int8_t priority) { header.priority = priority; }
		int priority() const { return header.priority; }
		
		bool operator<(const Message &m) const { return header.tsStart!=m.header.tsStart?header.tsStart<m.header.tsStart:header.tsEnd<m.header.tsEnd; }

		void ID(const std::string &ID) { for (uint i=0; i<sizeof(header.ID); i++) header.ID[i] = (i<ID.size() ? ID[i] : 0); }
		std::string ID() const { return std::string( header.ID, sizeof(header.ID)).c_str(); }

		object &operator()() { return *(object *)(data.get()); }
		const object &operator()() const { return *(object *)(data.get()); }

		const object operator[](int token) const { return (*this)().token(token); }
		Token operator[](int token) { return (*this)().token(token); }
				
		size_t size() const { return sizeof(Header)+(data?data->size():0); }
		
		template<typename T> 
		Message &operator<<(const T &v) { (*this)() << v; return *this; }
		
		template<typename T> 
		Message &operator>>(T &v) { (*this)() >> v; return *this; }
		
		void log(int level, bool showData=false) {
			
			Log(level) << "Message " << ID() << "(" << data->size() << ")" << " ts:" << (tsStart()-time_point(0_s)).count() << " " << (tsEnd()-time_point(0_s)).count() ; 
			if (showData) Log(level) << "  data: " << *data;
		}

		friend std::ostream &operator<<( std::ostream & os, const Message &m) {

			Log(-3) << "Writing package with ID:" << m.ID();
			Header h = m.header;
			h.size = m.data->size();
			if (not os.write( (char *)&h, sizeof(Header) ) ) throw std::runtime_error("Error Writting Header");
			if (not os.write( &m()[0],h.size)) throw std::runtime_error("Error Writting Message");
			os.flush();
			Log(-3) << "Package Wrote";
			return os;
		}

		friend std::istream &operator>>( std::istream & is, Message &m) {
					
			Log(-3) << "Reading pakcage";			
			if (not is) { m = Message(); return is; }
			if (not is.read( (char *)&m.header, sizeof(Header)) or is.gcount() != sizeof(Header) ) { m = Message(); return is; }
			if (m.header.magic != magic) throw std::runtime_error(object() << "Error Reading MAGIC number " << m.header.magic);
			m.data = std::make_shared<object>(m.header.size, 0);
			if (m.header.size != 0 and (not is.read( &m()[0], m.header.size) or uint(is.gcount()) != m.header.size ) ) throw std::runtime_error("Error Reading Message Data");	
			Log(-3) << "Read package with ID:" << m.ID();
			return is;
		}
	};
	
	class Queue;

	template<typename T>
	class iQueue {
		T &d() { return *static_cast<T*>(this); }
	public:
		
		bool pop( Message &m, nanoseconds timeout ) { return d().getB().pop(m, timeout); };
		bool pop( Message &m ) { return pop(m, 100_ms); };
		Message pop(nanoseconds timeout = 100_ms) { Message m; pop(m, timeout); return m; }

		bool empty() { return d().getB().empty(); }

		iQueue<Queue> &operator[](const std::string &id){ return d().getB()[id]; }

		T &operator>>( const std::string & );
		T &operator>>( Message &m ) { pop(m); return d(); }		
	};

	template<typename T>
	class oQueue {
		T &d() { return *static_cast<T*>(this); }
	public:

		bool push( const Message &m, nanoseconds timeout ) { return d().getA().push(m, timeout); }
		bool push( const Message &m ) { return push(m, 100_ms); }
		
		T &operator<<( const std::string & );
		T &operator<<( const Message &m ) { push(m); return d(); }
	};
	
	class Queue : public iQueue<Queue>, public oQueue<Queue>, private boost::noncopyable {
		// Parameters that determine the maximum size of the buffer: 
		// It is the maximum value between: maxSize and minNPkg*size_of_the_largest_received_message)
		size_t maxSize = 0;
		int minNPkg = 16;
		
		// Forced delay is used to give enough time to reorder packages in the buffer
		nanoseconds forcedDelay = 0_ms; 

		size_t size = 0;
		size_t nPkg = 0;
		std::list<Message> messages;

		// subQueue support
		std::shared_ptr<std::unordered_map<std::string,Queue>> subQueues;

		typedef std::unique_lock<std::mutex> Lock;
		std::mutex mtx;
		std::condition_variable cv;
		
		void addSorted( const Message &m ) {

			auto pos = messages.begin();
			while (pos!=messages.end() and m<*pos) pos++;
			messages.insert(pos,m);
			size += m.size();
			nPkg ++;
		}

		// Drop a random package from the lowest priority category while size is too big
		bool purge() {
			
			uint32_t fairRand = 2147483647;

			bool erased = false;
			while (size>maxSize) {

				int nPkg=1, lowestPriority = messages.front().priority();
				std::list<Message>::iterator loser;

				for (auto msg = messages.begin(); msg!=messages.end(); msg++) {

					if (msg->priority()<lowestPriority) nPkg = 1;

					lowestPriority = std::min(lowestPriority, msg->priority());

					if (msg->priority()==lowestPriority) 
						if ((fairRand%nPkg++)==0) 
							loser = msg;
				}
				nPkg --;
				size -= loser->size();

				Log(-1) << "Pkg with ID: " << loser->ID() << " was erased";

				messages.erase(loser);
				erased = true;
			}
			return erased;
		}

		friend oQueue<Queue>; Queue &getA() { return *this; }
		friend iQueue<Queue>; Queue &getB() { return *this; }

	public:
	
		using oQueue::push;
		bool push( const Message &m, nanoseconds timeout ) { 
			
			Lock l(mtx);
			if (subQueues and subQueues->count(m.ID())) return (*subQueues)[m.ID()].push(m,timeout);

			Log(-3) << "Pushed message: " << m.ID() << "(" << m().size() <<") with timeout: " << timeout.count();
			
			maxSize = std::max(maxSize, minNPkg*m.size());
			
			if (timeout!=0_s and size+m.size()>maxSize) 
				cv.wait_for(l, timeout, [&](){return size+m.size()<=maxSize;});

			addSorted(m);

			bool ret = purge();
			cv.notify_one();
			std::this_thread::yield();
			return ret;
		}

		using iQueue::pop;
		bool pop( Message &m, nanoseconds timeout ) {

			Lock l(mtx);
			Log(-3) << "Popping Message";
			if (empty() and (timeout==0_s or not cv.wait_for(l, timeout, [this](){return not empty();}))) {
				m = Message();
				return false;
			}

			m = messages.back(); 
			size -= m.size();
			nPkg --; 
			messages.pop_back();

			cv.notify_one();
			std::this_thread::yield();
			Log(-3) << "Popped message: " << m.ID() << "(" << m().size() <<")";
			return true;
		}

		iQueue<Queue> &operator [](const std::string &id){ 
			Lock l(mtx); 
			if (not subQueues) subQueues = std::make_shared<std::unordered_map<std::string,Queue>>(); 
			return (*subQueues)[id];
		}

		nanoseconds span() const { return messages.empty()?0_s:messages.front().tsStart()-messages.back().tsStart(); }

		bool empty() { return messages.empty() or span()<forcedDelay; }
	};
	
	class MemChannel : public iQueue<MemChannel>, public oQueue<MemChannel> {

		std::shared_ptr<Queue> queue = std::make_shared<Queue>();
		friend oQueue<Queue>; Queue &getA() { return *queue; }
		friend iQueue<Queue>; Queue &getB() { return *queue; }
		bool good = true;
	public:
		void close() { good = false; } 
		operator bool() const { return good; }			
	};
	
	namespace Net {
		
		class Connection : public iQueue<Connection>, public oQueue<Connection>, private boost::noncopyable {

			bool established = false;
			boost::asio::ip::tcp::socket socket;
			std::thread t;
				
			std::nullptr_t closeConnection() { watchDog.cancel(); keepAlive.cancel(); socket.close(); return nullptr; }

			std::shared_ptr<Queue> A;
			Message msgWrite;
			boost::asio::basic_waitable_timer< std::chrono::steady_clock > keepAlive;
			nanoseconds keepAliveTime = 100_ms;
			bool sending = false;
			void messageWriter() {

				if (not *this) return;				
				if (sending) return;
				keepAlive.expires_from_now(keepAliveTime);
				keepAlive.async_wait([this](const boost::system::error_code &ec){ 

					if (ec == boost::asio::error::operation_aborted) return;
					messageWriter();
				});

				Log(-2) <<  "Polling message to send";
				A->pop(msgWrite, 0_s);
				msgWrite.header.size = msgWrite().size();
				sending = true;
				Log(-2) <<  "Sending message " << msgWrite.ID() << "(" << msgWrite().size() << ")";
				
				msgWrite.header.size = msgWrite().size();
				boost::asio::async_write(socket,
					boost::asio::buffer((char *)&msgWrite.header, sizeof(msgWrite.header)),
					[this](boost::system::error_code ec, std::size_t length) {
					
					Log(-2) <<  "Header Sent";	
					if (ec) return Log(-1) <<  "Boost EC Error: " << ec.message() << " on line " << __LINE__ << closeConnection();
					if (length != sizeof(msgWrite.header) ) return Log(-1) <<  "Net: Error Writing Header" << closeConnection();
					
					boost::asio::async_write(socket,
						boost::asio::buffer(&msgWrite()[0], msgWrite.header.size),
						[this](boost::system::error_code ec, std::size_t length) {
						
						Log(-2) <<  "Body Sent";	
						if (ec) return Log(-1) <<  "Boost EC Error: " << ec.message() << " on line " << __LINE__ << closeConnection();
						if (length != msgWrite.header.size ) return Log(-1) << "Net: Error Writing Message Data" << closeConnection();
						
						sending = false;
						if (not A->empty()) messageWriter();
					});
				});
			}
			
			
			std::shared_ptr<Queue> B;
			Message msgRead;
			boost::asio::basic_waitable_timer< std::chrono::steady_clock > watchDog;
			nanoseconds watchDogTime = 500_ms;
			void messageReader() {
				
				if (not *this) return;
				watchDog.expires_from_now(watchDogTime);
				watchDog.async_wait([this](const boost::system::error_code &ec){ 

					if (ec == boost::asio::error::operation_aborted) return;
					closeConnection();
				});
				
				Log(-2) <<  "Start Reading Message";
				msgRead = Message();						
				boost::asio::async_read(socket,
					boost::asio::buffer((char *)&msgRead.header, sizeof(msgRead.header)),
					[this](const boost::system::error_code &ec, std::size_t length) {	
				
					if (ec) return Log(-1) <<  "Boost EC Error: " << ec.message() << " on line " << __LINE__ << closeConnection();
					if (length != sizeof(msgRead.header) ) return Log(-1) <<  "Net: Error Reading Header" << closeConnection() ;
					if (msgRead.header.magic != Message::magic) return Log(-1) <<  "Net: Error Reading MAGIC number " << msgRead.header.magic << closeConnection() ;
					
					Log(-2) <<  "Received header " << msgRead.ID() << "(" << msgRead.header.size << ")";
					if (not msgRead.header.size) {
						Log(-2) <<  "Read empty message";
						messageReader();
					} else {
						msgRead().resize(msgRead.header.size);
			
						boost::asio::async_read(socket,
							boost::asio::buffer(&msgRead()[0], msgRead.header.size),
							[this](const boost::system::error_code &ec, std::size_t length) {

							if (ec) return Log(-1) <<  "Boost EC Error: " << ec.message() << " on line " << __LINE__ << closeConnection();
							if (length != msgRead.header.size ) return Log(-1) << "Net: Error Message Data" << closeConnection();
							
							Log(-2) <<  "Received message " << msgRead.ID() << "(" << msgRead().size() << ")";
							B->push(msgRead,0_s);
							messageReader();
						});
					}
				});
			}
			
			friend iQueue<Connection>; decltype(*B) &getB() { return *B; }
			friend oQueue<Connection>; decltype(*A) &getA() { socket.get_io_service().post([this](){messageWriter();}); return *A; }

		public:
			Connection(boost::asio::ip::tcp::socket &socket, std::shared_ptr<Queue> A = std::make_shared<Queue>(), std::shared_ptr<Queue> B = std::make_shared<Queue>() ) :
				socket(socket.get_io_service()),
				A(A), keepAlive(socket.get_io_service()),
				B(B), watchDog(socket.get_io_service()) { 

				std::swap(this->socket, socket);

				socket.get_io_service().post([this](){messageReader();});
				socket.get_io_service().post([this](){messageWriter();});
			}
						
			~Connection() { closeConnection(); }
				
			operator bool() const { return socket.is_open(); }
		};

		class Server : public iQueue<Server>, public oQueue<Server>, private boost::noncopyable {
			
			boost::asio::io_service io_service;
			boost::asio::ip::tcp::acceptor acceptor;
			boost::asio::ip::tcp::socket socket;
			
			std::shared_ptr<Queue> B = std::make_shared<Queue>();
			std::list<Connection> connections;
			std::thread t;
			
			void addConnection() {
				
				acceptor.async_accept(
					socket, 
					[this](const boost::system::error_code &ec) { 
					
					Log(-1) << "Connection received!";
					if (ec) Log(-1) << "uSnippets::comm::Net::Server error: " << ec;
					if (not ec) connections.emplace_back(socket,std::make_shared<Queue>(),B);
					connections.remove_if( [](const Connection &element){ return not element; } );
					addConnection();
				});				
			}

			friend oQueue<Server>; Server &getA() { return *this; }
			friend iQueue<Server>; decltype(*B) &getB() { return *B; }
			
		public:

			Server(int port = 8888) :
				acceptor(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
				socket(io_service),
				t([this](){ 
					addConnection();
					Log(-2) << "Server: io_service starting"; 
					io_service.run(); 
					Log(-2) << "Server: io_service stopped"; 
				})
				{}
				
			~Server() {
				io_service.stop();
				if (t.joinable()) t.join();
			}

			using oQueue::push;
			bool push( const Message &m, nanoseconds timeout ) {
				Log(-2) << "Server: pushed"; 
				timeout = 0_s;	
				io_service.post([this,m,timeout](){
					for (auto &c : connections)
						if (c)
							c.push(m,timeout);
				});
				return true;
			}
		};
		
		class Client : public iQueue<Client>, public oQueue<Client>, private boost::noncopyable {

			typedef std::lock_guard<std::mutex> Lock;
			std::mutex mtx;
			
			boost::asio::io_service io_service;
			boost::asio::ip::tcp::resolver resolver;
			boost::asio::ip::tcp::socket socket;
			std::thread t;

			std::shared_ptr<Queue> A = std::make_shared<Queue>();
			std::shared_ptr<Queue> B = std::make_shared<Queue>();
			std::shared_ptr<Connection> connection;
			std::string host;
			int port;
			
			void resolveAndConnect() {

				Lock l(mtx);

				if (not io_service.stopped()) return;

				if (t.joinable()) t.join();
				io_service.reset();
				
				resolver.async_resolve(
					{ host, object(port) }, 
					[this](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {

					if (ec) return Log(-1) << "uSnippets::comm::Net::Client resolve error: " << ec << nullptr;
					Log(-2) << "Client Successfully Resolved";
					
					boost::asio::async_connect(
						socket, 
						endpoint_iterator,
						[this](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator) {
							
						if (ec) return Log(-1) << "uSnippets::comm::Net::Client connect error: " << ec << nullptr;
						Log(-2) << "Client Successfully Connected";

						Lock l(mtx);
						connection = std::make_shared<Connection>(socket,A,B);
					} );
				} );

				t = std::thread([this](){ 
					Log(-2) << "Client: io_service started"; 
					io_service.run(); 
					std::this_thread::sleep_for( 300_ms );
					Log(-2) << "Client: io_service stopped"; 
				});
			}
			
			friend oQueue<Client>; decltype(*A) &getA() { resolveAndConnect(); return *A; }
			friend iQueue<Client>; decltype(*B) &getB() { resolveAndConnect(); return *B; }
		public:
			Client(std::string host, int port=8888) : 
				resolver(io_service),
				socket(io_service),
				host(host), port(port) {
					io_service.stop();
					resolveAndConnect();
			}
			
			~Client() {
				io_service.stop();
				if (t.joinable()) t.join();
			}

			bool isConnected() { resolveAndConnect(); Lock l(mtx); return connection and *connection; }
//			operator bool() const { return not io_service.stopped(); }			
		};
	}
}
}
