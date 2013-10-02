/**
 * @file   pools-conn.cpp
 * @author Sergey Bryukov <sbryukov@gmail.com>
 * 
 * @brief  
 * 
 * 
 */

#include "pools-conn.hpp"

/** 
 * 
 * 
 * @param io_service 
 */
connection::connection(ba::io_service& io_service) : io_service_(io_service),
													 bsocket_(io_service),
													 ssocket_(io_service),
													 resolver_(io_service),
													 proxy_closed(false),
													 isPersistent(false),
													 isOpened(false) 
{
	miner_request_.reserve(8192);
}

/** 
 * Start read data of request from browser
 * 
 */
void connection::start() {
//  	std::cout << "start" << std::endl;
	miner_request_.clear();
	reqHeaders.clear();
	respHeaders.clear();
	
	handle_miner_read_headers(bs::error_code(), 0);
}

/** 
 * Read json message from miner
 * 
 * @param err 
 * @param len 
 */
void connection::handle_miner_read_headers(const bs::error_code& err, size_t len) {
 	if(!err) {

		if(miner_request_.empty())
			miner_request_=std::string(bbuffer.data(),len);
		else
			miner_request_+=std::string(bbuffer.data(),len);


		if(miner_request_.find("\n") == std::string::npos)
		{ // going to read rest of headers
			async_read(bsocket_, ba::buffer(bbuffer), ba::transfer_at_least(1),
					   boost::bind(&connection::handle_miner_read_headers,
								   shared_from_this(),
								   ba::placeholders::error,
								   ba::placeholders::bytes_transferred));
		} else { // analyze headers
			//std::cout << "miner_request_:\n" << miner_request_ << std::endl;
			std::string::size_type idx=miner_request_.find("\n");
			std::string reqString=miner_request_.substr(0,idx);
			miner_request_.erase(0,idx+1);

			parse_json_from_miner(reqString,reqHeaders);

			//===============================================
			//   bitcoind interface has not been defined yet
			//===============================================
			//start_connect_bitcoind();


			//==================================================================
			//   respons to miner  with default message with no any real jobs
			//==================================================================
			miner_request_ = "{\"id\":" + boost::lexical_cast<std::string>(mmainer_message_id_) + ", \"result\": null, \"error\": 25}\n";
			ba::async_write(bsocket_, ba::buffer(miner_request_),
										boost::bind(&connection::handle_miner_write,
													shared_from_this(),
													ba::placeholders::error,
													ba::placeholders::bytes_transferred));

		}
	} else {
		shutdown();
	}
}

/** 
 * Start connecting to the bitcoind
 * 
 */
void connection::start_connect_bitcoind() {
	std::string server="";
	std::string port="80";

	
	if(!isOpened || server != fServer_ || port != fPort_) {
		fServer_=server;
		fPort_=port;
		ba::ip::tcp::resolver::query query(server, port);
		resolver_.async_resolve(query,
								boost::bind(&connection::handle_resolve_bitcoind, shared_from_this(),
											boost::asio::placeholders::error,
											boost::asio::placeholders::iterator));
	} else {
	    start_write_to_bitcoind();
	}
}

/** 
 * If successful, after the resolved DNS-names of web-server into the IP addresses, try to connect
 * 
 * @param err 
 * @param endpoint_iterator 
 */
void connection::handle_resolve_bitcoind(const boost::system::error_code& err,
								ba::ip::tcp::resolver::iterator endpoint_iterator) {
//	std::cout << "handle_resolve. Error: " << err.message() << "\n";
    if (!err) {
		const bool first_time = true;
		handle_connect_bitcoind(boost::system::error_code(), endpoint_iterator, first_time);
    }else {
		shutdown();
	}
}

/** 
 * Try to connect to the web-server
 * 
 * @param err 
 * @param endpoint_iterator 
 */
void connection::handle_connect_bitcoind(const boost::system::error_code& err,
								ba::ip::tcp::resolver::iterator endpoint_iterator, const bool first_time) {
//	std::cout << "handle_connect. Error: " << err << "\n";
    if (!err && !first_time) {
		isOpened=true;
		start_write_to_bitcoind();
    } else if (endpoint_iterator != ba::ip::tcp::resolver::iterator()) {
		//ssocket_.close();
		ba::ip::tcp::endpoint endpoint = *endpoint_iterator;
		ssocket_.async_connect(endpoint,
							   boost::bind(&connection::handle_connect_bitcoind, shared_from_this(),
										   boost::asio::placeholders::error,
										   ++endpoint_iterator, false));
    } else {
		shutdown();
	}
}

/** 
 * Write data to the web-server
 * 
 */
void connection::start_write_to_bitcoind() {

	//fReq is Google Protocol Buffer
	ba::async_write(ssocket_, ba::buffer(fReq),
					boost::bind(&connection::handle_bitcoind_server_write, shared_from_this(),
								ba::placeholders::error,
								ba::placeholders::bytes_transferred));

	miner_request_.clear();
}

/** 
 * If successful, read the header that came from a web server
 * 
 * @param err 
 * @param len 
 */
void connection::handle_bitcoind_server_write(const bs::error_code& err, size_t len) {
// 	std::cout << "handle_server_write. Error: " << err << ", len=" << len << std::endl;
	if(!err) {
		handle_bitcoind_read_headers(bs::error_code(), 0);
	}else {
		shutdown();
	}
}

/** 
 * Read header of data returned from the web-server
 * 
 * @param err 
 * @param len 
 */
void connection::handle_bitcoind_read_headers(const bs::error_code& err, size_t len) {
// 	std::cout << "handle_server_read_headers. Error: " << err << ", len=" << len << std::endl;
	if(!err) {

			ba::async_write(bsocket_, ba::buffer(miner_request_),
							boost::bind(&connection::handle_miner_write,
										shared_from_this(),
										ba::placeholders::error,
										ba::placeholders::bytes_transferred));

	} else {
		shutdown();
	}
}

/** 
 * Writing data to the miner, are recieved from bitcooind
 * 
 * @param err 
 * @param len 
 */
void connection::handle_miner_write(const bs::error_code& err, size_t len) {
   	//std::cout << "handle_miner_write. Error: " << err << " " << err.message()
  	//		  << ", len=" << len << std::endl;
	if(!err) {
		if(!proxy_closed && (RespLen == -1 || RespReaded < RespLen))
			async_read(ssocket_, ba::buffer(sbuffer,len), ba::transfer_at_least(1),
					   boost::bind(&connection::handle_server_read_body,
								   shared_from_this(),
								   ba::placeholders::error,
								   ba::placeholders::bytes_transferred));
		else {
//			shutdown();
 			if(isPersistent && !proxy_closed) {
  				std::cout << "Starting read headers from browser, as connection is persistent" << std::endl;
  				start();
 			}
		}
	} else {
		shutdown();
	}
}

/** 
 * Reading data from a Web server, for the writing them to the browser
 * 
 * @param err 
 * @param len 
 */
void connection::handle_server_read_body(const bs::error_code& err, size_t len) {
//   	std::cout << "handle_server_read_body. Error: " << err << " " << err.message()
//  			  << ", len=" << len << std::endl;
	if(!err || err == ba::error::eof) {
		RespReaded+=len;
// 		std::cout << "len=" << len << " resp_readed=" << RespReaded << " RespLen=" << RespLen<< std::endl;
		if(err == ba::error::eof)
			proxy_closed=true;
		ba::async_write(bsocket_, ba::buffer(sbuffer,len),
						boost::bind(&connection::handle_miner_write,
									shared_from_this(),
									ba::placeholders::error,
									ba::placeholders::bytes_transferred));
	} else {
		shutdown();
	}
}

/** 
 * Close both sockets: for browser and web-server
 * 
 */
void connection::shutdown() {
	ssocket_.close();
	bsocket_.close();
}


void connection::parse_json_from_miner(const std::string& h, headersMap& hm) {
	std::string str(h);
	Json::Value root;   // will contains the root value after parsing.
	Json::Reader reader;
	bool parsingSuccessful = reader.parse( str, root );
	if ( !parsingSuccessful )
	{
	    // report to the user the failure and their locations in the document.
	    std::cout  << "Failed to parse configuration\n"
	               << reader.getFormattedErrorMessages();
	    return;
	}

	mmainer_message_id_ = root["id"].asInt();

	std::cout <<" -> id : " <<root["id"].asInt() << std::endl;

}
