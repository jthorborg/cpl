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

	file:Tools.h

		Implements specific functions to do work in GUI applications.
		For instance, main-thread safe async updates and such.

*************************************************************************************/

#ifndef CPL_TOOLS_H
#define CPL_TOOLS_H

#include <future>
#include <thread>
#include <set>

#include "../Common.h"
#include "../PlatformSpecific.h"
#include "../Misc.h"
#include "../Exceptions.h"
#include <mutex>

namespace cpl
{
	/// <summary>
	/// Very similar to Utility::DestructionServer, this is however one-way and more lightweight, AND
	/// - Requires the child class to call notifyListeners() in its destructor, such that
	/// (child*)this is a valid object on callback.
	/// </summary>
	class DestructionNotifier
	{
	public:
		class EventListener
		{
		public:
			virtual void onServerDestruction(DestructionNotifier * v) = 0;

			virtual ~EventListener() {};
		};

		virtual void addEventListener(EventListener * el) { std::lock_guard<std::mutex> lock(mutex); eventListeners.insert(el); }
		virtual void removeEventListener(EventListener * el) { std::lock_guard<std::mutex> lock(mutex); eventListeners.erase(el); }

		void notifyDestruction()
		{
			std::lock_guard<std::mutex> lock(mutex);
			for (auto listener : eventListeners)
				listener->onServerDestruction(this);
			eventListeners.clear();
		}

		virtual ~DestructionNotifier()
		{
			if (eventListeners.size())
			{
				// you must call notifyListeners() in your destructor!
				CPL_BREAKIFDEBUGGED();
				LogException("A destruction server didn't notify it's listeners upon destruction!");
			}
		}


	private:
		std::mutex mutex;
		std::set<EventListener *> eventListeners;
	};


	template<typename T>
	juce::Rectangle<int> centerRectInsideRegion(const juce::Rectangle<T> boundingRect, double length, double border)
	{
		// default to center top-left

		double newBorder = 0.5 * (boundingRect.getWidth() - length);
		if (newBorder > border)
		{
			return juce::Rectangle<double>
				(
					boundingRect.getX() + newBorder,
					boundingRect.getY() + newBorder,
					length,
					length
					).toType<int>();
		}
		else
		{
			double newLength = boundingRect.getWidth() - 2.0 * border;
			return juce::Rectangle<double>
				(
					boundingRect.getX() + border,
					boundingRect.getY() + border,
					newLength,
					newLength
					).toType<int>();
		}
	}

	namespace GUIUtils
	{

		class NestedMouseInterceptor : private juce::MouseListener, private juce::ComponentListener
		{
		public:

			class Listener
			{
			public:
				virtual void nestedOnMouseMove(const juce::MouseEvent& e) = 0;
				virtual void nestedOnMouseExit(const juce::MouseEvent& e) = 0;
				virtual ~Listener() {}
			};

			NestedMouseInterceptor(Listener * listener, juce::Component * source, bool wantsNestedEvents = true)
			{
				hook(listener, source, wantsNestedEvents);
			}

			NestedMouseInterceptor()
			{

			}

			void hook(Listener * listener, juce::Component * source, bool wantsNestedEvents = true)
			{
				CPL_RUNTIME_ASSERTION(listener != nullptr);
				CPL_RUNTIME_ASSERTION(source != nullptr);

				this->listener = listener;
				this->source = source;
				source->addMouseListener(this, wantsNestedEvents);
				source->addComponentListener(this);
			}

			void unhook()
			{
				if (source)
				{
					source->removeMouseListener(this);
					source->removeComponentListener(this);
				}

				listener = nullptr;
			}

			~NestedMouseInterceptor()
			{
				unhook();
			}

		private:

			virtual void componentBeingDeleted(juce::Component & c) override final
			{
				if (&c == source)
					source = nullptr;
			}

			virtual void mouseMove(const juce::MouseEvent & e) override final
			{
				listener->nestedOnMouseMove(e);
			}

			virtual void mouseExit(const juce::MouseEvent & e) override final
			{
				listener->nestedOnMouseExit(e);
			}

		private:
			Listener * listener = nullptr;
			juce::Component * source = nullptr;
		};


		// input polymorphic pointers!
		// returns true if the child is derived from the parent, or the parent contains the child somehow
		template<class Parent, class Child>
		inline bool ViewContains(const Parent * p, const Child * possibleChild)
		{
			return p == possibleChild || p->isParentOf(possibleChild);
		}
		template<class Functor>
		void AsyncCall(long msToDelay, Functor f)
		{
			std::async(std::launch::async, [=]()
			{
				cpl::Misc::Delay(static_cast<int>(msToDelay));
				const juce::MessageManagerLock lock;
				f();
			});
		}
		template<typename Functor>
		class DelayedCall : public juce::Timer, public DestructionNotifier::EventListener
		{
		public:
			DelayedCall(long numMs, Functor functionToRun)
				: func(functionToRun), contextWasDeleted(false), notifServer(nullptr)
			{
				startTimer((int)numMs);
			}

			DelayedCall(long numMs, Functor functionToRun, DestructionNotifier * server)
				: func(functionToRun), contextWasDeleted(false), notifServer(server)
			{
				notifServer->addEventListener(this);
				startTimer((int)numMs);
			}
			void timerCallback() override
			{
				stopTimer();
				if (!contextWasDeleted)
					func();
				delete this;
			}

			void onServerDestruction(DestructionNotifier * e) override
			{
				contextWasDeleted = true;
			}

			virtual ~DelayedCall()
			{
				if (!contextWasDeleted && notifServer)
				{
					notifServer->removeEventListener(this);
				}
			};
			Functor func;
			bool contextWasDeleted;
			DestructionNotifier * notifServer;
		};

		template<typename Functor>
		void RecurrentCallback(Functor func, long durationInMs, long numCalls, bool useMainThread = true)
		{
			class MainCallback : public juce::Timer
			{
			public:
				MainCallback(long frequencyMS, long numCalls)
					: frequency(frequencyMS), numCallsToMake(numCalls), currentCalls(0)
				{
					startTimer(frequency);
				}

				void timerCallback() override
				{
					func();
					currentCalls++;
					if (currentCalls >= numCallsToMake)
					{
						stopTimer();
						delete this;
					}
				}

			private:
				long frequency;
				long numCallsToMake;
				long currentCalls;
				Functor func;
			};

			class AsyncCallback
			{
			public:
				AsyncCallback(long frequencyMS, long numCalls)
					: frequency(frequencyMS), numCallsToMake(numCalls), currentCalls(0)
				{
					std::thread(run).detach();
				}

				void run()
				{
					while (numCallsToMake > currentCalls)
					{
						cpl::Misc::Delay(frequency);
						func();
						currentCalls++;
					}

					delete this;
				}

			private:
				long frequency;
				long numCallsToMake;
				long currentCalls;
				Functor func;
			};

			long numMsBetweenCalls = durationInMs / numCalls;
			if (useMainThread)
				new MainCallback(numMsBetweenCalls, numCalls);
			else
				new AsyncCallback(numMsBetweenCalls, numCalls);
		}


		template<typename Functor>
		void FutureMainEvent(long numMsToDelay, Functor functionToRun)
		{
			new DelayedCall<Functor>(numMsToDelay, functionToRun);
		}

		template<typename Functor>
		void FutureMainEvent(long numMsToDelay, Functor functionToRun, DestructionNotifier * notifServer)
		{
			if (notifServer)
			{
				new DelayedCall<Functor>(numMsToDelay, functionToRun, notifServer);
			}
		}

		template<typename Functor>
		void MainEvent(DestructionNotifier & notifServer, Functor functionToRun)
		{
			struct CustomMessage : public juce::MessageManager::MessageBase, public DestructionNotifier::EventListener
			{
				CustomMessage(Functor func, DestructionNotifier & server) : f(func), notif(&server) { notif->addEventListener(this); post(); }

				virtual void messageCallback() override { if (!contextWasDeleted) f(); }

				void onServerDestruction(DestructionNotifier * e) override { contextWasDeleted = true; }

				virtual ~CustomMessage()
				{
					if (!contextWasDeleted && notif)
					{
						notif->removeEventListener(this);
					}
				};

				Functor f;
				DestructionNotifier * notif;
				bool contextWasDeleted = false;
			};

			new CustomMessage(functionToRun, notifServer);
		}

		template<typename Functor>
		void MainEventBlocking(DestructionNotifier & notifServer, Functor functionToRun)
		{
			struct CustomMessage : public DestructionNotifier::EventListener
			{
				CustomMessage(Functor func, DestructionNotifier & server) : f(func), notif(&server) { notif->addEventListener(this); }

				void messageCallback() { if (!contextWasDeleted) f(); }

				void onServerDestruction(DestructionNotifier * e) override { contextWasDeleted = true; }

				virtual ~CustomMessage()
				{
					if (!contextWasDeleted && notif)
					{
						notif->removeEventListener(this);
					}
				};

				Functor f;
				DestructionNotifier * notif;
				bool contextWasDeleted = false;
			};

			CustomMessage m(functionToRun, notifServer);

			juce::MessageManager::getInstance()->callFunctionOnMessageThread(
				[](void* message) 
				{ 
					static_cast<CustomMessage*>(message)->messageCallback();
					return (void*)nullptr; 
				}, 
				static_cast<void*>(&m)
			);
		}

		template<typename Functor>
		void MainEvent(Functor functionToRun)
		{
			struct CustomMessage : public juce::MessageManager::MessageBase
			{
				CustomMessage(Functor func) : f(func) { post(); }

				virtual void messageCallback() override { f(); }

				Functor f;
			};

			new CustomMessage(functionToRun);
		}

		inline bool ForceFocusTo(const juce::Component & window)
		{
			#ifdef CPL_WINDOWS
			if (SetFocus((HWND)window.getWindowHandle()))
				return true;
			#else
			return false;
			#endif
			return false;
		}

		template<typename WindowPtrType>
		bool SynthesizeMouseClick(WindowPtrType windowHandle, int x, int y)
		{
			#ifdef CPL_WINDOWS
			#ifndef __SYNTHESISE_MOUSE
			INPUT input = {};
			input.type = INPUT_MOUSE;
			POINT clickPos = {x, y};
			if (!ClientToScreen(reinterpret_cast<HWND>(windowHandle), &clickPos))
				return false;


			input.mi.dx = clickPos.x;
			input.mi.dy = clickPos.y;
			input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
			if (!SendInput(1, &input, sizeof(INPUT)))
				return false;
			input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
			input.mi.time = 0;
			if (!SendInput(1, &input, sizeof(INPUT)))
				return false;
			#else
			PostMessage(
				reinterpret_cast<HWND>(windowHandle),
				WM_MOUSEMOVE,
				0,
				DWORD(uint16_t(x) | uint16_t(y) << 16)
			);
			PostMessage(
				reinterpret_cast<HWND>(windowHandle),
				WM_LBUTTONDOWN,
				0,
				DWORD(uint16_t(x) | uint16_t(y) << 16)
			);
			PostMessage(
				reinterpret_cast<HWND>(windowHandle),
				WM_LBUTTONUP,
				0,
				DWORD(uint16_t(x) | uint16_t(y) << 16)
			);
			#endif
			#else
			// UNIX
			return false;

			#endif
			return true;
		}
	};
};
#endif
