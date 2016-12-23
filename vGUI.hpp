/**
 * @file
 * @author  Manuel Martinez <manel@ira.uka.de>
 *
 */
 
#pragma once

#pragma GCC diagnostic ignored "-Winline"


#define GL_GLEXT_PROTOTYPES
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <uSnippets/object.hpp>
#include <uSnippets/GLFont.hpp>
#include <uSnippets/GLTex.hpp>

#include <SDL/SDL_stdinc.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>

#include <X11/Xlib.h>


#include <opencv/cv.h>

#include <mutex>
#include <thread>

namespace uSnippets {
namespace vGUI {

typedef cv::Rect_<cv::Vec2d> Frame;

class Widget {
	// Reverse container accessor
	template<typename T> struct reverseST {	T& c; 
		auto begin() -> decltype(c.rbegin()) { return c.rbegin(); }
		auto end()   -> decltype(c.rend())   { return c.rend();   }
	};
	template<typename T> reverseST<T> reverse(T&c) { return {c};}

	// Make Widgets non-copyable
	Widget( const Widget& );
	const Widget& operator=( const Widget& );
public:

	// Avoid multiple generation of the default font
	static GLFONT &defaultFont() { static GLFONT f; return f; }

	// Mutexes
	std::mutex mtx;
	typedef std::lock_guard<std::mutex> Lock;
	static std::unique_lock<std::recursive_mutex> getGlobalLock() { static std::recursive_mutex mtx; return std::unique_lock<std::recursive_mutex>(mtx); }

	// KEYBOARD MANAGEMENT
	static bool &keyStatus(int code) { static std::map<int, bool> ks; return ks[code]; }
	std::map<int, std::function<void()>> keyCBs;

	// Texture associated to the Widget
	GLTEX tex;

	// Obvious members
	bool visible = true;
	double renderingScale = 1.0; // Increase to sharpen the image, decrease for performance
	Frame frame = Frame(0,0,1,1);
	cv::Rect localRect, globalRect;
	std::map<std::string, std::shared_ptr<Widget>> children;


	//////////////////////////////////
	Widget(Frame frame = Frame(0,0,1,1)) : frame(frame) {}
	virtual ~Widget() {}

	std::function<void(cv::Size)> preDraw2D;
	std::function<void(cv::Size)> preDraw3D;
	std::function<void(cv::Size)> postDraw3D;
	std::function<void(cv::Size)> postDraw2D;

	std::function<void(bool &, cv::Point2i, cv::Point2i , int , SDL_Event)> processEvent = 
		[this](bool &occluded, cv::Point2i mA, cv::Point2i , int , SDL_Event){ occluded = occluded or globalRect.contains(mA); };

	template<typename T=Widget, typename ... Rest> std::shared_ptr<T> add(std::string key, Rest&&... rest) {

		auto l = getGlobalLock();
		auto p = key.find('/'); 
		if (p==std::string::npos) 
			return std::static_pointer_cast<T>( children[key] = std::make_shared<T>(rest...) );
		if (not children.count(key.substr(0,p))) children[key.substr(0,p)] = std::make_shared<Widget>();
		return children[key.substr(0,p)]->add<T>(key.substr(p+1), rest...);				
	}

	template<typename T=Widget> std::shared_ptr<T> get(std::string key) {

		auto l = getGlobalLock();
		auto p = key.find('/'); 
		if (p==std::string::npos)
			return std::static_pointer_cast<T>(children[key]);
		return children[key.substr(0,p)]->get<T>(key.substr(p+1));				
	}
protected:

	static void initGL(const cv::Size &sz, bool clear = true) {
		
		glViewport(0, 0, sz.width, sz.height); 
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel(GL_SMOOTH);
		glPolygonMode(GL_FRONT, GL_FILL);
		glPolygonMode(GL_BACK, GL_LINE);
		glColor4f(1.,1.,1.,1.);		
	}

	static void drawRecti(cv::Rect rect, bool flip=false) {
		glBegin(GL_QUADS);
		glTexCoord2f( 0, !!flip ); glVertex3f(rect.x             , rect.y              , 0 );
		glTexCoord2f( 0,  !flip ); glVertex3f(rect.x             , rect.y+rect.height-1, 0 );
		glTexCoord2f( 1,  !flip ); glVertex3f(rect.x+rect.width-1, rect.y+rect.height-1, 0 );
		glTexCoord2f( 1, !!flip ); glVertex3f(rect.x+rect.width-1, rect.y              , 0 );
		glEnd();
	}

	static void drawRectf(cv::Rect_<float> rect, bool flip=false) {
		glBegin(GL_QUADS);
		glTexCoord2f( 0, !!flip ); glVertex3f(rect.x           , rect.y            , 0 );
		glTexCoord2f( 0,  !flip ); glVertex3f(rect.x           , rect.y+rect.height, 0 );
		glTexCoord2f( 1,  !flip ); glVertex3f(rect.x+rect.width, rect.y+rect.height, 0 );
		glTexCoord2f( 1, !!flip ); glVertex3f(rect.x+rect.width, rect.y            , 0 );
		glEnd();
	}

	class FB {
		GLuint framebuffer;
		GLuint depth;
		GLTEX &tex;
	public:
		FB(const cv::Size &sz, GLTEX &tex) : tex(tex) {
			
			glGenFramebuffers(1, &framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

			if (tex.getSize() != sz) tex = GLTEX(sz);
			tex();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.name(), 0);

			// The depth buffer
			glGenRenderbuffers(1, &depth);
			glBindRenderbuffer(GL_RENDERBUFFER, depth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, sz.width, sz.height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

			if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Cannot Create Framebuffer Object");;
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			
			Widget::initGL(sz);
		}
		
		~FB() {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteRenderbuffers(1, &depth);
			glDeleteFramebuffers(1, &framebuffer);			
		}	
	};
	
	static void set2D(const cv::Size &sz) {
		
		glMatrixMode (GL_PROJECTION); glLoadIdentity();
		glOrtho(-.5, sz.width-.5, sz.height-.5, -.5, -1, 1);
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	}

	static void set3D(const cv::Size &sz) {
		
		glMatrixMode (GL_PROJECTION); glLoadIdentity();
		glTranslatef(sz.width/2.f, sz.height/2.f, 0);
		gluPerspective(60.0f,GLfloat(sz.width)/sz.height,1e-5f,1e3f);
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	}
	
	void update(cv::Rect parent) {
		
		auto l = getGlobalLock();

		localRect.x      = frame.x[0]      * parent.width  + frame.x[1];
		localRect.y      = frame.y[0]      * parent.height + frame.y[1];
		localRect.width  = frame.width[0]  * parent.width  + frame.width[1];
		localRect.height = frame.height[0] * parent.height + frame.height[1];

		globalRect = localRect+parent.tl();
		
		auto size = localRect.size();
	
		for (auto &f : reverse(children))
			if (f.second) 				
				f.second->update(globalRect);
 
		FB fb(cv::Size(size.width * renderingScale, size.height * renderingScale), tex);

		if (preDraw2D) { set2D(size); preDraw2D(size); }

		if (preDraw3D) { set3D(size); preDraw3D(size); }

		set2D(size);
		for (auto &f : reverse(children)) {
			if (f.second and f.second->visible) {
				f.second->tex();
//				std::cout << "Local Size: " << size.width << " " << size.height << std::endl; 
//				std::cout << f.first << " " << f.second->localRect.x << " " << f.second->localRect.y << " " << f.second->localRect.width << " " << f.second->localRect.height << std::endl;
				drawRecti(f.second->localRect, true);				
			}
		}

		if (postDraw3D) { set3D(size); postDraw3D(size); }

		if (postDraw2D) { set2D(size); postDraw2D(size); }
	}
	
	void recursiveEvent(bool &occluded, cv::Point2i mA, cv::Point2i mR, int mS, SDL_Event event) {
		
		auto l = getGlobalLock();

		if (event.type==SDL_KEYDOWN and keyCBs[event.key.keysym.sym]) keyCBs[event.key.keysym.sym]();

		for (auto &f : children)
			if (f.second)
				f.second->recursiveEvent(occluded, mA, mR, mS, event);

		if (processEvent) processEvent(occluded, mA, mR, mS, event);
	}
};
/*
.0
	struct ZoomWidget : public Widget {
		
		bool enableZoom, enablePan;
		cv::Point3f pos, goal;

		ZoomWidget(Rectf frame = Rectf(0,0,1.-1e-3,1.-1e-3)) : Widget(), enableZoom(0), enablePan(0), pos(0,0,1), goal(0,0,1) {}

		virtual void GLpreDraw2D(cv::Rect localFrame) {
		
			glDisable(GL_TEXTURE_2D); 
			glColor3f(.25,.25,.25);
			glBegin(GL_LINES);
			for (int i=localFrame.x; i<localFrame.x+localFrame.width; i+=80) {
				glVertex2i(i,localFrame.y);
				glVertex2i(i,localFrame.y+localFrame.height);
			}
			for (int i=localFrame.y; i<localFrame.y+localFrame.height; i+=80) {
				glVertex2i(localFrame.x, i);
				glVertex2i(localFrame.x+localFrame.width, i);
			}
			glEnd();
			glColor3f(1,1,1);
		}
		

		virtual void draw(cv::Rect parent, int stencil = 0) {

			float speed = .8;
			pos = pos*speed + goal*(1-speed);
			glMatrixMode(GL_PROJECTION);
			glTranslatef(pos.x, pos.y, 0);
			glScalef(pos.z, pos.z, pos.z);
			Widget::draw(parent, stencil);
		}
			
		virtual bool event(cv::Rect parent, int x, int y, int rx, int ry, int mouseStatus, std::map<int, bool> &keyStatus, SDL_Event event) {

			//Lock l(mtx());
			cv::Rect localFrame = getLocalFrame(parent);
			
			// CTRL+mousewheel: Zoom
			if (enableZoom and localFrame.contains({x,y}) and  keyStatus[SDLK_LCTRL] and !keyStatus[SDLK_LSHIFT] and event.type == SDL_MOUSEBUTTONUP and event.button.button == SDL_BUTTON_WHEELDOWN) { goal.z *= (1/1.08); goal.x = x-parent.x-(x-parent.x-goal.x)*(1/1.08); goal.y = y-parent.y-(y-parent.y-goal.y)*(1/1.08);}
			if (enableZoom and localFrame.contains({x,y}) and  keyStatus[SDLK_LCTRL] and !keyStatus[SDLK_LSHIFT] and event.type == SDL_MOUSEBUTTONUP and event.button.button == SDL_BUTTON_WHEELUP  ) { goal.z *=    1.08 ; goal.x = x-parent.x-(x-parent.x-goal.x)*(  1.08); goal.y = y-parent.y-(y-parent.y-goal.y)*(  1.08);}
			// SHIFT+mousewheel: Pan left/right
			if (enablePan and localFrame.contains({x,y}) and !keyStatus[SDLK_LCTRL] and  keyStatus[SDLK_LSHIFT] and event.type == SDL_MOUSEBUTTONUP and event.button.button == SDL_BUTTON_WHEELDOWN) { goal.x += 24;}
			if (enablePan and localFrame.contains({x,y}) and !keyStatus[SDLK_LCTRL] and  keyStatus[SDLK_LSHIFT] and event.type == SDL_MOUSEBUTTONUP and event.button.button == SDL_BUTTON_WHEELUP  ) { goal.x -= 24;}
			// mousewheel: Pan up/down
			if (enablePan and localFrame.contains({x,y}) and !keyStatus[SDLK_LCTRL] and !keyStatus[SDLK_LSHIFT] and event.type == SDL_MOUSEBUTTONUP and event.button.button == SDL_BUTTON_WHEELDOWN) { goal.y += 24;}
			if (enablePan and localFrame.contains({x,y}) and !keyStatus[SDLK_LCTRL] and !keyStatus[SDLK_LSHIFT] and event.type == SDL_MOUSEBUTTONUP and event.button.button == SDL_BUTTON_WHEELUP  ) { goal.y -= 24;}
			
			// Middle: mousepan
			if (localFrame.contains({x,y}) and event.type == SDL_MOUSEMOTION and (((SDL_MouseMotionEvent *)&event)->state&SDL_BUTTON_MMASK) ) { goal.x += ((SDL_MouseMotionEvent *)&event)->xrel; goal.y += ((SDL_MouseMotionEvent *)&event)->yrel; pos = goal;}		

			goal.z = std::min(std::max(goal.z,1.f),100.f);
			goal.x = std::min(std::max(goal.x,-parent.width *(goal.z-1)),0.f);
			goal.y = std::min(std::max(goal.y,-parent.height*(goal.z-1)),0.f);

			pos.z = std::min(std::max(pos.z,1.f),100.f);
			pos.x = std::min(std::max(pos.x,-parent.width *(pos.z-1)),0.f);
			pos.y = std::min(std::max(pos.y,-parent.height*(pos.z-1)),0.f);

			if (localFrame.contains({x,y}) and event.type==SDL_MOUSEBUTTONDOWN) return false;
			if (localFrame.contains({x,y}) and event.type==SDL_MOUSEBUTTONUP)   return false;

			double lx = (x-parent.x-pos.x)/pos.z;
			double ly = (y-parent.y-pos.y)/pos.z;

			for (auto &f : children)
				if (f.second and f.second->event(localFrame, lx, ly, rx, ry, mouseStatus, keyStatus, event))
					return true;
			return false;
		}
	};

	struct CanvasWidget : public Widget {
		
		cv::Mat3b img;
		GLTEX tex;
		CanvasWidget(Rectf frame = Rectf(0,0,1.-1e-3,1.-1e-3)) : Widget(frame), img(cv::Mat3b(1024,1024,cv::Vec3b(255,255,255))), tex(img)  {}
		
		virtual void GLpostDraw2D(cv::Rect localFrame) {
			
			tex();
			glBegin(GL_QUADS);
			glTexCoord2f( 0, 0 ); glVertex3f(                 10,                  10, 0. );
			glTexCoord2f( 0, 1 ); glVertex3f(                 10,localFrame.height-10, 0. );
			glTexCoord2f( 1, 1 ); glVertex3f(localFrame.width-10,localFrame.height-10, 0. );
			glTexCoord2f( 1, 0 ); glVertex3f(localFrame.width-10,                  10, 0. );
			glEnd();
		}
			
		virtual bool event(cv::Rect parent, int x, int y, int rx, int ry, int mouseStatus, std::map<int, bool> &keyStatus, SDL_Event event) {
			
			cv::Rect localFrame = getLocalFrame(parent);
			cv::circle(img, cv::Point2d(img.cols*(x-localFrame.x-10)/double(1e-5+localFrame.width-20), img.rows*(y-localFrame.y-10)/double(1e-5+localFrame.height-20)), 24, cv::Scalar(0,0,255),-1);
			tex = img;
			return false;
		}
	};
*/


struct Image : public Widget {
	
	GLTEX img;
	
	Image(const GLTEX &t, Frame frame = Frame(0,0,1,1)) : Widget(frame), img(t) {
		
		postDraw2D = [this](cv::Size sz) {
			
			if (not sz.width or not sz.height) return;
			Lock l(this->mtx);
			if (not img.getSize().width or not img.getSize().height) return;
			double ratio = double(img.getSize().width*sz.height)/(img.getSize().height*sz.width);
			
			glTranslatef(sz.width/2.,sz.height/2.,0);
			glScalef(sz.width*std::min(ratio, 1.),sz.height*std::min(1/ratio, 1.),1.);
						
			img();
			drawRectf({-.5,-.5,1.,1.});
		};		
	}
	
	void updateImg(const GLTEX &t) {

		Lock l(this->mtx);
		img = t;		
	}
};

template<class T, class D = decltype(T()-T())>
struct Slider : public Widget {
	
	D knobWidth;
	T start, end;
	T pos;
	
	cv::Rect sliderROI, knobROI;
	
	bool knobPicked = false;
	double knobPickedOff = 0;
	
	std::function<void()> updateSlider;
	
	
	Slider(Frame frame = Frame(0,{1,-16},1,{0,16})) : Widget(frame) {
		
		postDraw2D = [this](cv::Size sz) {

			sliderROI = cv::Rect()+sz;

			glDisable(GL_TEXTURE_2D); 
			glColor4f(.75,.75,.75,1.); drawRecti(sliderROI);

			if (end<=start) return;

			pos = std::min(end,std::max(start,pos));
			
			knobROI = cv::Rect()+sz;
			
			knobROI.width *= knobWidth/(end-start+knobWidth); 
			knobROI.width = std::max(knobROI.width,knobROI.height/2); 
			knobROI.x += (sz.width-knobROI.width)*(pos-start)/(end-start); // unsure
			
			glColor4f(.25,.25,.25,1.); drawRecti(knobROI);
			glColor4f(1.,1.,1.,1.);
		};
		
		processEvent = [&](bool &occluded, cv::Point2i mA, cv::Point2i mR, int mS, SDL_Event event) {

			cv::Point2i rmA = mA-globalRect.tl();

			if (not occluded and sliderROI.contains(rmA) and event.type==SDL_MOUSEBUTTONDOWN) {
				if  (not knobROI.contains(rmA)) {

					knobPicked = true;
					knobPickedOff = knobROI.width/2;
				} else {
					knobPicked = true;
					knobPickedOff = rmA.x-knobROI.x;
				}
			}
			if (knobPicked) {
				pos = start+(rmA.x-knobPickedOff)*(end-start)/(sliderROI.width-knobROI.width+1e-10);
				pos = std::min(end,std::max(start,pos));
			}

			if (event.type==SDL_MOUSEBUTTONUP) {
				knobPicked = false;
			}

			if (event.type == SDL_USEREVENT and updateSlider)
				updateSlider();
			
			occluded = occluded or globalRect.contains(mA);
		};
	}
};


struct Label : public Widget {
	
	std::string label;
	GLFONT font;
		
	Label(Frame frame = Frame(0,0,{0,16},{0,16}), std::string label = "Label", GLFONT font = Widget::defaultFont()) : Widget(frame), label(label), font(font) {
		
		renderingScale = 2;
		
		postDraw2D = [this](cv::Size sz) {
			
			glDisable(GL_TEXTURE_2D); 
			glColor4f(1.,1.,1.,1.);
			glTranslatef(sz.width/2,sz.height/2,0);
			glScalef(sz.height*.9, sz.height*.9, 1);
			this->font.drawCentered(this->label);
		};		
	}
};

struct Button : public Widget {
	
	cv::Rect ROI;

	std::string label;
	std::function<void()> f;
	GLFONT font;
	
	bool start;
	bool toggled = false; // Toggle drawing included here for brevity reasons
	bool hovering = false;
	bool pressed = false;
		
	Button(Frame frame = Frame(0,0,{0,16},{0,16}), std::string label = "Button", std::function<void()> f = std::function<void()>(), GLFONT font = Widget::defaultFont()) : Widget(frame), label(label), f(f), font(font) {
		
		renderingScale = 2;
		
		postDraw2D = [this](cv::Size sz) {
			
			ROI = cv::Rect()+sz;			
			glDisable(GL_TEXTURE_2D); 
			glColor3f(.25,.25,.25);
			drawRecti(ROI);

			glColor3f(.5,.5,.5);
			if (hovering) glColor3f(.6,.6,.6);
			if (toggled) glColor3f(.4,.4,.4);
			if (pressed) glColor3f(.3,.3,.3);
			drawRecti(ROI+cv::Size(-4,-4)+cv::Point2i(2,2));

			glColor4f(1.,1.,1.,1.);
			glTranslatef(sz.width/2,sz.height/2,0);
			glScalef(sz.height*.9, sz.height*.9, 1);
			this->font.drawCentered(this->label);
		};
		
		processEvent = [this](bool &occluded, cv::Point2i mA, cv::Point2i mR, int mS, SDL_Event event) {

			cv::Point2i rmA = mA-globalRect.tl();
			
			hovering = ROI.contains(rmA); 
			start = (start and (mS & SDL_BUTTON_LMASK)) or (not pressed and hovering and (mS & SDL_BUTTON_LMASK));
			pressed = hovering and (mS & SDL_BUTTON_LMASK);

			if (occluded) return;
			if (start and hovering and event.type == SDL_MOUSEBUTTONUP and mS == SDL_BUTTON_LMASK) if (this->f) this->f();

			occluded = occluded or globalRect.contains(mA);
		};
	}
};

struct ToggleButton : public Button {
	
	ToggleButton(Frame frame = Frame(0,0,{0,16},{0,16}), std::string label = "Button", std::function<void()> f = std::function<void()>(), GLFONT font = Widget::defaultFont()) : 
		Button(frame, label, f, font) {
		
		processEvent = [this](bool &occluded, cv::Point2i mA, cv::Point2i mR, int mS, SDL_Event event) {

			cv::Point2i rmA = mA-globalRect.tl();
			
			hovering = ROI.contains(rmA); 			
			start = (start and (mS & SDL_BUTTON_LMASK)) or (not pressed and hovering and (mS & SDL_BUTTON_LMASK));
			pressed = hovering and (mS & SDL_BUTTON_LMASK);

			if (occluded) return;
			if (start and hovering and event.type == SDL_MOUSEBUTTONUP and mS == SDL_BUTTON_LMASK) {
				
				toggled = !toggled;
				if (this->f) this->f();
			}

			occluded = occluded or globalRect.contains(mA);
		};
	}
};

struct SidePanel : public Widget {
	
	SidePanel(Frame frame = Frame(1,0,{0,400},1)) : Widget(frame) {

		preDraw2D = [this](cv::Size sz) {
			
			glDisable(GL_TEXTURE_2D); 
			glColor4f(.75,.75,.75,1.); drawRecti(cv::Rect()+sz);
			glColor4f(1.,1.,1.,1.);
		};
		
		processEvent = [this](bool &occluded, cv::Point2i mA, cv::Point2i mR, int mS, SDL_Event event) {

			if (globalRect.contains(mA)) {
				this->frame.x[1] += (-this->frame.width[1]-this->frame.x[1])*.2;
			} else {
				this->frame.x[1] += (-10-this->frame.x[1])*.1;
			}

			occluded = occluded or globalRect.contains(mA);
		};
	}
};

struct DefaultComposer : public Widget {
	
	DefaultComposer() {
		
		add<Slider<double>>("10Slider",Frame(0,{1,-16},1,{0,16}));

		cv::Mat1b check = (cv::Mat1b(4,4) << 0,0,1,0,0,1,0,1,1,0,1,0,0,1,0,1);
		check *= 255;

		add<Image>("05Image",check,Frame( 0, 0,.5, {1,-16}));
		add<Image>("06Image",check,Frame(.5, 0,.5, {1,-16}));

		add<SidePanel>("00SidePanel");
		add<Image>("00SidePanel/05Image",check,Frame( 0, 0, 1, 1));
		add<Button>("00SidePanel/00Button",Frame( 0, 0, 1, {0,32}), "Button");
	}
};
	
template<typename T> 
class BaseWindow : public T {

	// WINDOW MANAGEMENT
	SDL_Surface *surface = nullptr;
	std::string windowTitle;
	cv::Size windowSize,fullscreenSize;
	bool isFullscreen = false;
	bool isGood = true;
	
	// WINDOWS UPDATE
	double targetFPS = 30.;

	// OPENGL MANAGEMENT
	void initWindow() {
		
		if (surface) 
			SDL_FreeSurface(surface);

		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 8);

		if (isFullscreen)
			surface = SDL_SetVideoMode( fullscreenSize.width, fullscreenSize.height, 32, SDL_OPENGL|SDL_FULLSCREEN );
		else
			surface = SDL_SetVideoMode( windowSize.width, windowSize.height, 32, SDL_OPENGL|SDL_RESIZABLE );
		
		if (not surface) {
			
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0);

			if (isFullscreen)
				surface = SDL_SetVideoMode( fullscreenSize.width, fullscreenSize.height, 32, SDL_OPENGL|SDL_FULLSCREEN );
			else
				surface = SDL_SetVideoMode( windowSize.width, windowSize.height, 32, SDL_OPENGL|SDL_RESIZABLE );
		}
		
		if (not surface) throw std::runtime_error("Not able to create GL surface");
			
		SDL_WM_SetCaption( windowTitle.c_str(), NULL );
	}
	
	void processIO() {
		
		SDL_Event event;
		event.type = SDL_USEREVENT;

		cv::Point2i mA, mR;
		int mS = SDL_GetMouseState(&mA.x, &mA.y);
		SDL_GetRelativeMouseState(&mR.x, &mR.y);
		
		bool occluded = false;
		this->recursiveEvent(occluded, mA, mR, mS, event);

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				isGood = false;
				break;
			case SDL_VIDEORESIZE:
				windowSize = cv::Size(event.resize.w,event.resize.h);
				initWindow();
				break;
			case SDL_KEYDOWN:
				this->keyStatus(event.key.keysym.sym) = true;
				break;
			case SDL_KEYUP:
				this->keyStatus(event.key.keysym.sym) = false;
				break;
			}
			
			occluded = false;
			this->recursiveEvent(occluded, mA, mR, mS, event);
		}
	}

	// MAIN OPENGL THREAD
	std::thread t;
	void thread() { // MAIN OPENGL/SDL THREAD
		
		// INIT SCREEN
		XInitThreads();
		SDL_Init( SDL_INIT_EVERYTHING );
		
		// CALC SCREEN SIZE
		const SDL_VideoInfo* videoInfo = SDL_GetVideoInfo();
		fullscreenSize = cv::Size(videoInfo->current_w, videoInfo->current_h);
		windowSize = cv::Size(videoInfo->current_w*.8, videoInfo->current_h*.8);
		
		initWindow();

		auto lastUpdate = std::chrono::system_clock::now();
		this->update(cv::Rect(0,0,surface->w, surface->h));
		while (isGood) {
			
			std::this_thread::sleep_until( lastUpdate + std::chrono::microseconds(uint64_t(1.e6/targetFPS)) );
			lastUpdate = std::chrono::system_clock::now();

			processIO();

			this->update(cv::Rect(0,0,surface->w, surface->h));

			Widget::initGL(cv::Size(surface->w, surface->h), true);
			this->tex();
			this->drawRecti(this->localRect,true);
			SDL_GL_SwapBuffers();
//			std::cout << "Updated" << std::endl;
		}
		SDL_Quit();
	}

public:

	BaseWindow(std::string windowTitle = "Video GUI", bool disabled=false) : 
		windowTitle(windowTitle),
		t([this, disabled](){if (not disabled) this->thread();}) { 
				
		this->keyCBs['q'] = [this](){ isGood=false; };
		this->keyCBs[SDLK_ESCAPE] = [this](){ isGood=false; };
		this->keyCBs[SDLK_F11   ] = [this](){ isFullscreen=!isFullscreen; initWindow(); };
	}
	
	~BaseWindow() {
		
		isGood=false;
		t.join(); 
	}
	
	operator bool() { return isGood; }
	
	void run() { while (isGood) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
};

typedef BaseWindow<DefaultComposer> Window;
}
}
