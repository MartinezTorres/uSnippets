////////////////////////////////////////////////////////////////////////
// web uGui2D backend
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once

#include <uSnippets/object.hpp>
#include <uSnippets/mjpeg.hpp>
#include <uSnippets/gui2d.hpp>
#include <uSnippets/time.hpp>
#include <uSnippets/log.hpp>
#include <uSnippets/turbojpeg.hpp>

#include <opencv2/opencv.hpp>


namespace uSnippets {
struct Gui2Dweb : private boost::noncopyable {

	static std::string base64_encode(const std::string &in) {

		std::string out;

		int val=0, valb=-6;
		for (uchar c : in) {
			val = (val<<8) + c;
			valb += 8;
			while (valb>=0) {
				out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
				valb-=6;
			}
		}
		if (valb>-6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
		while (out.size()%4) out.push_back('=');
		return out;
	}

	static std::string base64_decode(const std::string &in) {

		std::string out;

		std::vector<int> T(256,-1);
		for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i; 
		
		int val=0, valb=-8;
		for (uchar c : in) {
			if (T[c] == -1) break;
			val = (val<<6) + T[c];
			valb += 6;
			if (valb>=0) {
				out.push_back(char((val>>valb)&0xFF));
				valb-=8;
			}
		}
		return out;
	}
	
	const char *endl = "\r\n";
	
	std::string title;
	Mjpeg mjpg;

	struct Gui2Dm : public Gui2D {
		
		uSnippets::time_point lastAccessed;
		std::string sessionId;
		Mjpeg &mjpg;
		std::thread t;
		std::function<void(cv::Mat3b)> camCB;
		
		Gui2Dm(const std::string &sessionId, Mjpeg &mjpg, std::function<void(Gui2Dm&)> f) : lastAccessed(now()), sessionId(sessionId), mjpg(mjpg), t([this,f](){f(*this);}) {};
		
		virtual void img( const cv::Mat3b &img )  { mjpg.add(showImg = img, sessionId); }
	};
	
	std::map<std::string, std::shared_ptr<Gui2Dm>> sessions;
	std::function<void(Gui2Dm&)> f;
	
	Gui2Dweb(const std::string &title, int port, std::function<void(Gui2Dm&)> f) : title(title), mjpg(port,95), f(f) {

		mjpg("", [this](boost::asio::ip::tcp::iostream &net, std::string , std::string url, std::string ) {
			
			if (url!="/") return;
			
			std::string sessionId;
			do sessionId = object(10000+rand()%89999); while ( sessions.count(sessionId)!= 0);
			
			net << "HTTP/1.1 200 OK" << endl;
			net << "Content-Type: text/html" << endl << endl;
			net << "<meta http-equiv=\"refresh\" content=\"0; url=/home/" << sessionId << "/\">" << endl;
			net << "<script> window.location.href = \"/home/" << sessionId << "\"</script>" << endl << endl;
			
			auto &session = sessions[sessionId]; if (not session) session = std::make_shared<Gui2Dm>(sessionId, mjpg, this->f);
			session->lastAccessed = now();			
		});

		mjpg("home", [this](boost::asio::ip::tcp::iostream &net, std::string , std::string url, std::string ) {
		
			std::string sessionId = getUrlToken(url,1);

			net << "HTTP/1.1 200 OK" << endl;
			net << "Content-Type: text/html" << endl << endl;
net << R"(
<html style="height:100%">
<head>
	<script class="jsbin" src="/jquery/script"></script>
	<title> )" << this->title << R"( </title>
</head>


<body id='background' style="height:100%" >
	<div style="height:100%"></div>
	<div style="display: none;"> <video id="video" width="640" height="480" autoplay></video> <canvas id="canvas" width="640" height="480"></canvas> </div>
</body>

<script>
$(document).ready(function () { $('#background').mousemove(function (e) { $.post('/mousemove/)" << sessionId << R"(/'+(e.clientX - $(this).offset().left)/$(this).width()+'/'+(e.clientY - $(this).offset().top)/$(this).height()+'/'+$(this).width()/$(this).height()+'/'); }); });
$(document).ready(function () { $('#background').mousedown(function (e) { $.post('/mousedown/)" << sessionId << R"(/'+(e.clientX - $(this).offset().left)/$(this).width()+'/'+(e.clientY - $(this).offset().top)/$(this).height()+'/'+$(this).width()/$(this).height()+'/'); }); });
$(document).ready(function () { $('#background').mouseup(function (e)   { $.post('/mouseup/)"   << sessionId << R"(/'+(e.clientX - $(this).offset().left)/$(this).width()+'/'+(e.clientY - $(this).offset().top)/$(this).height()+'/'+$(this).width()/$(this).height()+'/'); }); });
$(document).ready(function () { $('#background').dblclick(function (e)  { $.post('/dblclick/)"  << sessionId << R"(/'+(e.clientX - $(this).offset().left)/$(this).width()+'/'+(e.clientY - $(this).offset().top)/$(this).height()+'/'+$(this).width()/$(this).height()+'/'); }); });
$(document).ready(function () { $('#background').keypress(function (e)  { $.post('/keypress/)"  << sessionId << R"(/'+e.keyCode+'/'); }); });

window.addEventListener("DOMContentLoaded", function() {

	var canvas = document.getElementById("canvas"),
		context = canvas.getContext("2d"),
		video = document.getElementById("video"),
		videoObj = { "video": true },
		errBack = function(error) { console.log("Video capture error: ", error.code); };

	if(navigator.getUserMedia) { // Standard
		navigator.getUserMedia(videoObj, function(stream) {
			video.src = stream;
			video.play();
		}, errBack);
	} else if(navigator.webkitGetUserMedia) { // WebKit-prefixed
		navigator.webkitGetUserMedia(videoObj, function(stream){
			video.src = window.webkitURL.createObjectURL(stream);
			video.play();
		}, errBack);
	}
	else if(navigator.mozGetUserMedia) { // Firefox-prefixed
		navigator.mozGetUserMedia(videoObj, function(stream){
			video.src = window.URL.createObjectURL(stream);
			video.play();
		}, errBack);
	}

	function sendFrame() {
		setTimeout(function() {
			document.getElementById("canvas").getContext("2d").drawImage(video, 0, 0, 640, 480); 
			$.post('/cam/)" << sessionId << R"(/'+canvas.toDataURL('image/jpeg',.90)+'/');
			sendFrame();
		}, 30);
	}
				
	video.onloadeddata = sendFrame;

	setTimeout(function() {
		$('#background').css("background", "#000000 no-repeat center center url(/mjpeg/)" << sessionId << R"(/) ");
		$('#background').css("background-size", "contain");
	}, 1000);

}, false);
</script>
</html>	
)" << endl;
		});
		
		
		mjpg("mousemove", [&](boost::asio::ip::tcp::iostream &net, std::string, std::string url, std::string ) {
		
			net << "HTTP/1.1 200 OK" << endl << "Content-Type: text/html" << endl << endl << "<html></html>" << endl << endl;
			
			std::string sessionId = getUrlToken(url,1);
			auto &session = sessions[sessionId]; if (not session) session = std::make_shared<Gui2Dm>(sessionId, mjpg, this->f);
			session->lastAccessed = now();
			session->eventMouseMoved(cv::Point2f(getUrlToken(url,2), getUrlToken(url,3)));
		});

		mjpg("mousedown", [&](boost::asio::ip::tcp::iostream &net, std::string, std::string url, std::string ) {
		
			net << "HTTP/1.1 200 OK" << endl << "Content-Type: text/html" << endl << endl << "<html></html>" << endl << endl;
			
			std::string sessionId = getUrlToken(url,1);
			auto &session = sessions[sessionId]; if (not session) session = std::make_shared<Gui2Dm>(sessionId, mjpg, this->f);
			session->lastAccessed = now();
			session->eventMousePressed(cv::Point2f(getUrlToken(url,2), getUrlToken(url,3)));
		});

		mjpg("mouseup", [&](boost::asio::ip::tcp::iostream &net, std::string, std::string url, std::string ) {
		
			net << "HTTP/1.1 200 OK" << endl << "Content-Type: text/html" << endl << endl << "<html></html>" << endl << endl;
			
			std::string sessionId = getUrlToken(url,1);
			auto &session = sessions[sessionId]; if (not session) session = std::make_shared<Gui2Dm>(sessionId, mjpg, this->f);
			session->lastAccessed = now();
			session->eventMouseReleased(cv::Point2f(getUrlToken(url,2), getUrlToken(url,3)));
		});

		mjpg("dblclick", [&](boost::asio::ip::tcp::iostream &net, std::string, std::string url, std::string ) {
		
			net << "HTTP/1.1 200 OK" << endl << "Content-Type: text/html" << endl << endl << "<html></html>" << endl << endl;
			
			std::string sessionId = getUrlToken(url,1);
			auto &session = sessions[sessionId]; if (not session) session = std::make_shared<Gui2Dm>(sessionId, mjpg, this->f);
			session->lastAccessed = now();
			session->eventDblclk(cv::Point2f(getUrlToken(url,2), getUrlToken(url,3)));
		});
		
		mjpg("cam", [&](boost::asio::ip::tcp::iostream &net, std::string, std::string url, std::string ) {
		
			net << "HTTP/1.1 200 OK" << endl << "Content-Type: text/html" << endl << endl << "<html></html>" << endl << endl;
			
			std::string sessionId = getUrlToken(url,1);
			auto &session = sessions[sessionId]; if (not session) session = std::make_shared<Gui2Dm>(sessionId, mjpg, this->f);
			session->lastAccessed = now();
			if (session->camCB) session->camCB(TJ::decode3b(base64_decode(url.substr(7+url.find("base64")))));			
		});		
	}

	static object getUrlToken(const std::string &url, int i) {
		
		std::string out;
		for (size_t c=1; c<url.size() and i>=0; c++) 
			if (url[c]=='/') i--; else if (not i) out+=url[c]; 
		return out;
	}
	/*
	static cv::Point2f getCoords( std::string url ) {
		
		std::replace(url.begin(), url.end(), '/', ' ');
		std::istringstream in(url);
		std::string command, id;
		double x=0, y=0, ratio=0, rows=showImg.rows, cols=showImg.cols;
		if (in >> command >> id >> x >> y >> ratio) {
			if (not ratio or not rows) return cv::Point(0,0);
			if (ratio>cols/rows) {
				x = x*ratio*rows-(ratio*rows-cols)/2;
				y *= rows;
			} else {
				y = y*cols/ratio-(cols/ratio-rows)/2;
				x *= cols;
			}
		}
		return cv::Point(x,y);
	}*/

};
}
