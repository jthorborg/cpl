/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2016 Janus Lynggaard Thorborg (www.jthorborg.com)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	See \licenses\ for additional details on licenses associated with this program.

**************************************************************************************

	file:CViews.h

		Provides some base classes for views, that automatically supports
		openGL rendering, notification of death, serialization etc.

*************************************************************************************/

#ifndef CPL_CVIEWS_H
#define CPL_CVIEWS_H


#include <set>
#include "../Common.h"
#include "CToolTip.h"
#include "GraphicComponents.h"
#include "CEditSpaceSpawner.h"
#include "../rendering/OpenGLRendering.h"
#include "../Protected.h"
#include "Tools.h"
#include "../state/Serialization.h"
#include "../profiling/Profiling.h"

namespace cpl
{

	/// <summary>
	/// A CView is the base class of all views.
	/// </summary>
	class CView
		:
		public SafeSerializableObject,
		virtual public DestructionNotifier,
		public Utility::CNoncopyable
	{

	public:


		CView(std::string name)
			: isFullScreen(false)
			, isSynced(false)
			, oglc(nullptr)
			, bufferSwapInterval(0)
			, refreshRate(0)
			, viewName(std::move(name))
		{
		}


		virtual ~CView()
		{

		}
		const std::string & getName() const noexcept { return viewName; }
		virtual juce::Component * getWindow() = 0;
		virtual bool setFullScreenMode(bool toggle) { isFullScreen = toggle; return false; }
		bool getIsFullScreen() const { return isFullScreen; }
		virtual void repaintMainContent() {};
		virtual void visualize() {};
		/// <summary>
		/// The view should stop any processing.
		/// </summary>
		virtual void suspend() {};
		/// <summary>
		/// Indicates the view should resume any previous processing
		/// </summary>
		virtual void resume() {  };
		/// <summary>
		/// Indicates that the view should not react to new audio
		/// </summary>
		virtual void freeze() {};
		/// <summary>
		/// Inverse of freeze
		/// </summary>
		virtual void unfreeze() {};
		/// <summary>
		/// Called when process-specific buffers (delay-lines etc.) and variables should be reset to a default state.
		/// It doesn't indicate to reset the program or settings.
		/// </summary>
		virtual void resetState() {};
		virtual void attachToOpenGL(juce::OpenGLContext & ctx) { detachFromOpenGL();  oglc = &ctx; }
		virtual void detachFromOpenGL(juce::OpenGLContext & ctx)
		{
			if (oglc)
				jassert(&ctx == oglc);
			ctx.detach();
			oglc = nullptr;
		}
		void detachFromOpenGL() { if (oglc) oglc->detach(); oglc = nullptr; }

		bool isOpenGL() const noexcept { return oglc != nullptr; }
		juce::OpenGLContext * getAttachedContext() const noexcept { return oglc; }
		bool shouldSynchronize() { return isSynced; }
		void setApproximateRefreshRate(int ms) { refreshRate = ms; }
		void setSwapInterval(int interval) { bufferSwapInterval = interval; }
		int getSwapInterval() const noexcept { return isOpenGL() ? oglc->getSwapInterval() : 1; }

	protected:

		bool isFullScreen;
		bool isSynced;
		// the rate at which the 2D ui gets refreshed (called from repaintMainContent)
		int refreshRate;
		// -1 == no swap buffer interval defined, all opengl rendering
		// should be triggered through repaintMainContent()
		// 0 == no cap on framerate, 1 = vsync, 2 and over are reciprocals of
		// current monitor refresh rate.
		int bufferSwapInterval;

		juce::OpenGLContext * oglc;
		std::string viewName;
	};

	/*
		A subview is a stand-alone view, instantiable as-is.
	*/
	class CSubView
		:
		public CView,
		public juce::Component
	{
	public:

		CSubView(std::string name)
			: CView(std::move(name))
		{

		}

		juce::Component * getWindow() override { return this; }

	};

	/*
		An openGL view is able to draw openGL through the renderOpenGL()
		function and standard 2D graphics through paint().
		Note that openGL may not be enabled, and you probably should
		enable software rendering as well - in paint(). You can test
		for openGL availability through isOpenGL().
	*/
	class COpenGLView
		:
		public CSubView,
		juce::OpenGLRenderer
	{

	public:

		class OpenGLEventListener
		{
		public:
			virtual void onOGLRendering(COpenGLView * view) {};
			virtual void onOGLContextCreation(COpenGLView * view) { };
			virtual void onOGLContextDestruction(COpenGLView * view) {};
			virtual ~OpenGLEventListener() {}
		};

		COpenGLView(std::string name, std::shared_ptr<Profiling::Lane> profilerLane = nullptr)
			: CSubView(std::move(name))
			, profilerLane(profilerLane)
		{
			graphicsStamp = juce::Time::getHighResolutionTicks();
			openGLStamp = juce::Time::getHighResolutionTicks();
			graphicsDelta = openGLDelta = 0.0;
			openGlEndToEnd = 0;
			graphicsEndToEnd = 0;
		}

		/// <summary>
		/// Repaints the main content. Use this for updating the 2D graphics juce system,
		/// or periodically, if the view isn't continously repainting.
		/// </summary>
		void repaintMainContent() override
		{
			repaint();
			if (isOpenGL() && bufferSwapInterval < 0)
				oglc->triggerRepaint();
		}

		virtual void initOpenGL() {}
		virtual void closeOpenGL() {}
		virtual void onOpenGLRendering() {}
		virtual void onGraphicsRendering(juce::Graphics & g) {}

		/// <summary>
		/// Instructs the OpenGLContext to render this view, along with some other
		/// work behind the scenes. Drop-in replacement for OpenGLContext::attachTo()
		/// </summary>
		/// <param name="ctx"></param>
		void attachToOpenGL(juce::OpenGLContext & ctx) override
		{
			ctx.setRenderer(this);
			CView::attachToOpenGL(ctx);
			ctx.attachTo(*this);
		}

		void detachFromOpenGL(juce::OpenGLContext & ctx) override
		{
			CView::detachFromOpenGL(ctx);
			ctx.setRenderer(nullptr);
		}

		/// <summary>
		/// Returns the time it took from the last frame started rendering, till this frame started rendering.
		/// This can be used as a fraction of how much time you should proceed in this frame.
		/// Time is in seconds.
		/// </summary>
		inline double openGLDeltaTime() const
		{
			return openGLDelta;
		}

		/// <summary>
		/// Returns the end-to end time it takes between rendering the last openGL frames.
		/// Can be used to calculate frames per second
		/// </summary>
		inline double openGlEndToEndTime() const
		{
			return openGlEndToEnd;
		}

		/// <summary>
		/// Returns the time it took to render the last openGL frame.
		/// </summary>
		inline double openGLFrameTime() const
		{
			return openGLFrame;
		}

		/// <summary>
		/// Returns the time it took from the last frame started rendering, till this frame started rendering
		/// This can be used as a fraction of how much time you should proceed in this frame.
		/// Time is in seconds.
		/// </summary>
		/// <returns></returns>
		inline double graphicsDeltaTime() const
		{
			return graphicsDelta;
		}

		/// <summary>
		/// Returns the time it took to render the last 2d graphics frame.
		/// </summary>
		inline double graphicsFrameTime() const
		{
			return graphicsFrame;
		}

		/// <summary>
		/// Returns the end-to end time it takes between rendering the last openGL frames.
		/// Can be used to calculate frames per second
		/// </summary>
		inline double graphicsGlEndToEndTime() const
		{
			return graphicsEndToEnd;
		}

		void addOpenGLEventListener(OpenGLEventListener * listener)
		{
			std::lock_guard<std::mutex> lock(hookMutex);
			oglEventListeners.insert(listener);
		}

		void removeOpenGLEventListener(OpenGLEventListener * listener)
		{
			std::lock_guard<std::mutex> lock(hookMutex);
			oglEventListeners.erase(listener);
		}

	protected:

		void renderOpenGL() override final
		{
			#ifdef CPL_TRACEGUARD_ENTRYPOINTS
			CPL_TRACEGUARD_START
			#endif

			Profiling::ProfilerFrame frame(profilerLane.get());
			frame.setWork(1); // one frame.

			{
				CPL_PROFILE("OpenGLView::renderingHook");

				std::lock_guard<std::mutex> lock(hookMutex);
				for (auto && l : oglEventListeners)
					l->onOGLRendering(this);
			}

			juce::OpenGLHelpers::resetErrorState();			
			auto start = juce::Time::getHighResolutionTicks();
			/// <summary>
			/// If the stack gets corrupted, the next variable should not have been overwritten, and can be used
			/// for debugging
			/// </summary>
			#if !defined(CPL_CLANG) && (defined(DEBUG) || defined(_DEBUG))
			volatile CPL_THREAD_LOCAL COpenGLView * _stackSafeThis = this;
			(void)_stackSafeThis;
			#endif
			openGLDelta = juce::Time::highResolutionTicksToSeconds(start - openGLStamp);

			CPL_DEBUGCHECKGL();

			CPL_PROFILE("OpenGLView::renderOpenGL");
			onOpenGLRendering();

			CPL_DEBUGCHECKGL();

			auto oldStamp = openGLStamp;
			openGLStamp = juce::Time::getHighResolutionTicks();
			openGlEndToEnd = juce::Time::highResolutionTicksToSeconds(openGLStamp - oldStamp);
			openGLFrame = juce::Time::highResolutionTicksToSeconds(openGLStamp - start);
			#ifdef CPL_TRACEGUARD_ENTRYPOINTS
			CPL_TRACEGUARD_STOP("OpenGL rendering entry");
			#endif
		}

		void paint(juce::Graphics & g) override final
		{
			// TODO: Probably remove this function.
			auto start = juce::Time::getHighResolutionTicks();
			graphicsDelta = juce::Time::highResolutionTicksToSeconds(start - graphicsStamp);
			onGraphicsRendering(g);
			auto oldStamp = graphicsStamp;
			graphicsStamp = juce::Time::getHighResolutionTicks();
			graphicsFrame = juce::Time::highResolutionTicksToSeconds(graphicsStamp - start);
			graphicsEndToEnd = juce::Time::highResolutionTicksToSeconds(graphicsStamp - oldStamp);
		}

		/// <summary>
		/// This can be called during openGL rendering to compose juce graphics
		/// directly to the openGL surface, at a specific point in time. Notice the call doesn't happen 
		/// on the main thread.
		/// </summary>
		template<typename ThreadedGraphicsFunction>
		void renderGraphics(ThreadedGraphicsFunction func)
		{
			if (!oglc)
				CPL_RUNTIME_EXCEPTION("OpenGL graphics composition called without having a target context.");

			CPL_DEBUGCHECKGL();


			// TODO: consider if the graphics context can be created/acquired somehow else, so we don't have to consider screen size.. etc.
			auto scale = oglc->getRenderingScale();
			std::unique_ptr<juce::LowLevelGraphicsContext> context;
			
			{
				CPL_PROFILE("OpenGLView::create2DGraphics");
				context = juce::createOpenGLGraphicsContext(
						*oglc,
						static_cast<int>(scale * getWidth()),
						static_cast<int>(scale * getHeight())
				);
			}


			juce::Graphics g(*context);
			if (scale != 1.0)
				g.addTransform(AffineTransform::scale((float)scale));


			CPL_DEBUGCHECKGL();

			CPL_PROFILE("OpenGLView::paint2DGraphics");
			func(g);

			CPL_DEBUGCHECKGL();
		}

		void newOpenGLContextCreated() override
		{
			{
				std::lock_guard<std::mutex> lock(hookMutex);
				for (auto && l : oglEventListeners)
					l->onOGLContextCreation(this);
			}
			initOpenGL();
		}
		void openGLContextClosing() override
		{
			{
				std::lock_guard<std::mutex> lock(hookMutex);
				for (auto && l : oglEventListeners)
					l->onOGLContextDestruction(this);
			}
			closeOpenGL();
		}

		virtual ~COpenGLView()
		{
			CView::detachFromOpenGL();
		}

	protected:
		cpl::relaxed_atomic<double> graphicsDelta, graphicsFrame, graphicsEndToEnd, openGLDelta, openGLFrame, openGlEndToEnd;

		decltype(juce::Time::getHighResolutionTicks()) graphicsStamp;
		decltype(juce::Time::getHighResolutionTicks()) openGLStamp;

	private:
		std::mutex hookMutex;
		std::set<OpenGLEventListener *> oglEventListeners;
		std::shared_ptr<Profiling::Lane> profilerLane;
	};

	/*
		A top-view is a view, that also handles tooltips and edit spaces
		for it's children. Has an associative style.
	*/
	class CTopView
		:
		public CView
	{

	public:
		CTopView(juce::Component * parent, std::string name)
			: CView(std::move(name)), isTooltipsOn(false), tipWindow(nullptr), editSpawner(*parent)
		{
			parent->setLookAndFeel(&CLookAndFeel_CPL::defaultLook());
		}

	protected:

	private:
		bool isTooltipsOn;
		cpl::CEditSpaceSpawner editSpawner;
		cpl::CToolTipWindow tipWindow;
	};
};
#endif
