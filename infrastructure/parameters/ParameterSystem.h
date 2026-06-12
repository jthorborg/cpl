#ifndef CPL_PARAMETERSYSTEM_H
#define CPL_PARAMETERSYSTEM_H

#include <atomic>
#include <string>
#include "../../ConcurrentServices.h"
#include <vector>
#include <array>
#include "../../gui/Tools.h"
#include "CustomFormatters.h"
#include "CustomTransforms.h"
#include "../../state/Serialization.h"

namespace cpl
{
	template<typename T>
	class ZeroOneClamper
	{
	public:
		T operator()(T arg) { return std::clamp(arg, (T)0, (T)1); }
	};


	// Concecpt of Parameters: has T getValue(), setValue(T), std::string getName()
	template<typename T, class TransformerType = VirtualTransformer<T>, class Restricter = ZeroOneClamper<T>, std::memory_order LoadOrdering = std::memory_order_relaxed, std::memory_order StoreOrdering = std::memory_order_relaxed>
	class /* alignas(CPL_CACHEALIGNMENT) */ ThreadedParameter : Utility::COnlyPubliclyMovable
	{
	public:

		typedef TransformerType Transformer;
		typedef T ValueType;

		ThreadedParameter(std::string name, Transformer & parameterTransformer)
			: transformer(parameterTransformer), name(std::move(name)), value(0)
		{

		}

		ThreadedParameter(ThreadedParameter && other) noexcept
			: value(other.value.load(LoadOrdering)), transformer(other.transformer), name(std::move(other.name))
		{

		}

		T getValue() const { return value.load(LoadOrdering); }
		void setValue(T newValue) { value.store(Restricter()(newValue), StoreOrdering); }

		const std::string& getName() { return name; }
		Transformer & getTransformer() { return transformer; }
		const Transformer & getTransformer() const { return transformer; }

	private:

		std::atomic<T> value;
		Transformer & transformer;
		std::string name;

	};

	template<typename T, class BaseParameter, class FormatterType = VirtualFormatter<T>>
	class FormattedParameter
		: public BaseParameter
		, private BasicFormatter<T> // (compression/EBO)
	{
	public:
		typedef FormatterType Formatter;

		FormattedParameter(std::string name, typename BaseParameter::Transformer & transformer, Formatter * formatterToUse = nullptr)
			: BaseParameter(std::move(name), transformer), formatter(formatterToUse)
		{

		}

		Formatter & getFormatter() { return formatter ? *formatter : *this; }
		bool setFormatter(Formatter & formatterToUse) { return formatter = &formatterToUse; }

	private:
		Formatter * formatter;
	};


	namespace Parameters
	{
		typedef int Handle;

		typedef int UpdateFlagsT;

		enum UpdateFlags : UpdateFlagsT
		{
			/// <summary>
			/// Nothing recieves a notification
			/// </summary>
			None = 1 << 0,
			/// <summary>
			/// Any real-time listeners that immediately receives a notification
			/// </summary>
			RealTimeListeners = 1 << 1,
			/// <summary>
			/// Whatever realtime system is there, receives a notification.
			/// For instance, the audio thread (and the host)
			/// </summary>
			RealTimeSubSystem = 1 << 2,
			/// <summary>
			/// UI receives a notification
			/// </summary>
			UI = 1 << 3,
			/// <summary>
			/// Everything receives a notification
			/// </summary>
			All = UI | RealTimeSubSystem | RealTimeListeners
		};

		class UserContent
		{
		public:
			virtual ~UserContent() {};
		};

		template<typename ParameterView>
		struct CallbackParameterRecord
		{
			typename ParameterView::ParameterType * parameter;
			bool shouldBeAutomatable;
			bool canChangeOthers;
			Parameters::Handle handle;
			ParameterView * uiParameterView;
		};

		class CallbackRecordInterface
		{
		public:
			/// <summary>
			/// A callback when all parameters (including any nested ones) has been installed.
			/// At this point, Entry[n].uiParameterView is a stable reference for the life time of the parameter system
			/// </summary>
			virtual void parametersInstalled() = 0;
			/// <summary>
			/// Called just before any parameter(s) are queried
			/// </summary>
			virtual void generateInfo() {};
			virtual ~CallbackRecordInterface() {}
		};

		template<typename ParameterView>
		class BundleUpdate : public CallbackRecordInterface
		{
		public:
			typedef Parameters::CallbackParameterRecord<ParameterView> Record;
			typedef BundleUpdate<ParameterView> QualifedBundle;

			/// <summary>
			/// Returns a short, semantic name for this group of parameters (for instance, a widget name
			/// containing them)
			/// </summary>
			virtual const std::string & getBundleContext() const noexcept = 0;
			/// <summary>
			/// Queries a list of parameters, and fills in a reference to a ParameterView.
			/// Returned data must be valid until parametersInstalled() has been called.
			/// </summary>
			virtual std::vector<Record> & queryParameters() = 0;

			/// <summary>
			/// If this bundle contains other owned bundles, it can override this function to return
			/// non-null
			/// </summary>
			virtual std::vector<QualifedBundle *> * getNestedChilds() { return nullptr; }

		};

		template<typename ParameterView>
		class SingleUpdate : public CallbackRecordInterface
		{
		public:
			/// <summary>
			/// Initialize to a valid object after generateInfo() has happened. Not needed after parametersInstalled().
			/// </summary>
			Parameters::CallbackParameterRecord<ParameterView> * parameterQuery;
		};

	};

	template<class T, typename InternalFrameworkType, typename BaseParameterT>
	class ParameterGroup
		: public CSerializer::Serializable
		, private DestructionNotifier::EventListener
	{
	public:

		typedef BaseParameterT BaseParameter;
		typedef typename BaseParameter::Transformer Transformer;
		typedef typename BaseParameter::Formatter Formatter;
		typedef InternalFrameworkType FrameworkType;


		typedef ParameterGroup<T, InternalFrameworkType, BaseParameter> QualifiedGroup;

		static const Parameters::Handle InvalidHandle = -1;

		class AutomatedProcessor
		{
		public:
			/// <summary>
			/// The sematics of this function is to transmit the change message,
			/// but not proceed with any setParameter() calls
			/// </summary>
			virtual void automatedTransmitChangeMessage(int parameter, FrameworkType value) = 0;
			virtual void automatedBeginChangeGesture(int parameter) = 0;
			virtual void automatedEndChangeGesture(int parameter) = 0;
			virtual ~AutomatedProcessor() {}
		};

		class ParameterView;

		/// <summary>
		/// UI listeners recieve parameter notification only for registred controls through the UI thread.
		/// </summary>
		class UIListener
		{
		public:
			virtual void parameterChangedUI(Parameters::Handle localHandle, Parameters::Handle globalHandle, ParameterView* parameter) = 0;
			virtual ~UIListener() {}
		};

		/// <summary>
		/// RT listeners recieve notifictions of all parameter changes immediately.
		/// Unlike UI listeners, RT listeners are not guaranteed to be notified of all changes 
		/// </summary>
		class RTListener
		{
		public:
			virtual void parameterChangedRT(Parameters::Handle localHandle, Parameters::Handle globalHandle, BaseParameter* param) = 0;
			virtual ~RTListener() {}
		};

		class ParameterView
		{
		public:
			friend class ParameterGroup;
			typedef UIListener Listener;
			typedef T ValueType;
			typedef BaseParameter ParameterType;
			ParameterView(
				QualifiedGroup* parentToRef,
				BaseParameter* parameterToRef,
				Parameters::Handle handleOfThis,
				bool paramIsAutomatable = true,
				bool paramCanChangeOthers = false,
				std::string nameContext = ""
			)
				: parent(parentToRef)
				, handle(handleOfThis)
				, parameter(parameterToRef)
				, isAutomatable(paramIsAutomatable)
				, canChangeOthers(paramCanChangeOthers)
				, nameContext(std::move(nameContext))
			{

			}

			ParameterView(ParameterView&& other)
				: parent(other.parent)
				, handle(other.handle)
				, parameter(other.parameter)
				, isAutomatable(other.isAutomatable)
				, canChangeOthers(other.canChangeOthers)
				, nameContext(std::move(other.nameContext))
			{

			}


			ParameterType* getParameter() noexcept { return parameter; }
			const std::string& getNameContext() const { return nameContext; }
			std::string getExportedName() { return parent->prefix + nameContext + parameter->getName(); }
			const std::string& getLocalName() { return parameter->getName(); }
			const std::string& getParentPrefix() const { return parent->getExportPrefix(); }
			Parameters::Handle getHandle() { return handle; }
			void addListener(UIListener* listener) { parent->addUIListener(handle, listener); }
			void removeListener(UIListener* listener) { parent->removeUIListener(handle, listener); }

			void updateFromUINormalized(ValueType value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
			{
				parent->updateFromUINormalized(handle, value, flags);
			}

			void updateFromProcessorNormalized(ValueType value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
			{
				parent->updateFromProcessorNormalized(handle, value, flags);
			}

			void updateFromHostNormalized(ValueType value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
			{
				parent->updateFromHostNormalized(handle, value, flags);
			}

			void beginChangeGesture() { parent->beginChangeGesture(handle); }
			void endChangeGesture() { parent->endChangeGesture(handle); }

			void updateFromUITransformed(ValueType value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
			{
				return updateFromUINormalized(parameter->getTransformer().normalize(value), flags);
			}

			void updateFromProcessorTransformed(ValueType value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
			{
				return updateFromProcessorNormalized(parameter->getTransformer().normalize(value), flags);
			}

			bool updateFromUIStringTransformed(const std::string_view value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
			{
				T interpretedValue;
				if (parameter->getFormatter()->interpret(value, interpretedValue))
				{
					return updateFromUINormalized(parameter->getTransformer().normalize(interpretedValue), flags);
				}
				return false;
			}

			template<typename Ret = T>
			Ret getValueNormalized() const { return static_cast<Ret>(parameter->getValue()); }
			template<typename Ret = T>
			Ret getValueTransformed() const { return static_cast<Ret>(parameter->getTransformer().transform(parameter->getValue())); }


			std::string getDisplayText()
			{
				std::string buf;
				parameter->getFormatter().format(parameter->getTransformer().transform(parameter->getValue()), buf);
				return buf;
			}

			Formatter& getFormatter() { return parameter->getFormatter(); }
			Transformer& getTransformer() { return parameter->getTransformer(); }

			bool isParameterAutomated() const { return isAutomatable; }
			bool canParameterChangeOthers() const { return canChangeOthers; }

		private:

			std::string nameContext;
			Parameters::Handle handle;
			BaseParameter* parameter;
			bool isAutomatable;
			bool canChangeOthers;
			ABoolFlag changedFromProcessor;
			std::set<UIListener*> uiListeners;
			QualifiedGroup* parent;
		};



		ParameterGroup(std::string name, std::string exportPrefix, AutomatedProcessor& processorToAutomate, int parameterOffset = 0)
			: processor(processorToAutomate), offset(parameterOffset), groupName(std::move(name)), prefix(std::move(exportPrefix)), isSealed(false)
		{
			bundleInstalledReferences = std::make_unique<std::vector<BundleInstallReference>>();
			singleInstalledReferences = std::make_unique<std::vector<SingleInstallReference>>();
		}

		int getOffset() const noexcept { return offset; }

		void serialize(CSerializer::Archiver& archive, Version v) override
		{
			if (!isSealed)
				CPL_RUNTIME_EXCEPTION("Parameter system being serialized without being sealed");

			archive << groupName;
			archive << offset;

			for (std::size_t i = 0; i < containedParameters.size(); ++i)
			{
				archive << containedParameters[i].getValueNormalized();
			}
		}

		void deserialize(CSerializer::Builder& builder, Version v) override
		{
			if (!isSealed)
				CPL_RUNTIME_EXCEPTION("Parameter system being deserialized without being sealed");

			builder >> groupName;
			builder >> offset;

			for (std::size_t i = 0; i < containedParameters.size(); ++i)
			{
				T value;
				builder >> value;
				containedParameters[i].updateFromUINormalized(value);
			}
		}

		/// <summary>
		/// Sets some user-generated content to be associated with this parameter group.
		/// The content ownership can be transferred to the parameter group (default).
		/// You can for instance store all your parameters, formatters and transformers in a 
		/// class deriving from the Parameters::UserContent.
		/// </summary>
		void setUserData(Parameters::UserContent* content, bool releaseOnDestruction = true)
		{
			userContent.reset(content);
			userContent.get_deleter().doDelete = !!releaseOnDestruction;
		}

		/// <summary>
		/// Returns anything previously set with setUserData() - as of such, may be null.
		/// </summary>
		Parameters::UserContent* getUserContent() noexcept
		{
			return userContent.get();
		}

		/// <summary>
		/// This function must only be called during initialization, ie. before any audio callbacks are done.
		/// Additionally, it only makes sense to call it on the UI thread.
		/// </summary>
		Parameters::Handle registerParameter(BaseParameter* param, bool shouldBeAutomatable = true, bool canChangeOthers = false, std::string nameContext = "")
		{
			if (isSealed)
				CPL_RUNTIME_EXCEPTION("Parameters registered to the system while it's sealed");
			auto pos = containedParameters.size();
			containedParameters.emplace_back(this, param, (Parameters::Handle)(pos + offset), shouldBeAutomatable, canChangeOthers, nameContext);
			return static_cast<Parameters::Handle>(pos + offset);
		}


		void registerParameterBundle(Parameters::BundleUpdate<ParameterView>* bundle, std::string contextStack = "")
		{
			contextStack += bundle->getBundleContext();
			bundle->generateInfo();
			auto& parameters = bundle->queryParameters();
			for (auto& parameter : parameters)
			{
				parameter.handle = registerParameter(parameter.parameter, parameter.shouldBeAutomatable, parameter.canChangeOthers, contextStack);
			}

			bundleInstalledReferences.get()->push_back({ bundle, &parameters });

			if (auto bundleChilds = bundle->getNestedChilds())
			{
				for (auto& childBundle : *bundleChilds)
				{
					registerParameterBundle(childBundle, contextStack);
				}
			}

		}

		void registerSingleParameter(Parameters::SingleUpdate<ParameterView>* singleRef)
		{
			singleRef->generateInfo();
			singleRef->parameterQuery->handle = registerParameter(
				singleRef->parameterQuery->parameter,
				singleRef->parameterQuery->shouldBeAutomatable,
				singleRef->parameterQuery->canChangeOthers
			);
			singleInstalledReferences.get()->push_back({ singleRef, singleRef->parameterQuery });
		}

		void seal()
		{
			isSealed = true;
			for (auto& ref : *bundleInstalledReferences.get())
			{
				for (auto& parameter : *ref.records)
				{
					parameter.uiParameterView = findParameter(parameter.handle);
				}

				ref.parent->parametersInstalled();
			}

			for (auto& ref : *singleInstalledReferences.get())
			{
				ref.record->uiParameterView = findParameter(ref.record->handle);
				ref.parent->parametersInstalled();
			}

			bundleInstalledReferences = nullptr;
			singleInstalledReferences = nullptr;

		}

		/// <summary>
		/// Only safe to call on the UI thread.
		/// </summary>
		void addUIListener(Parameters::Handle globalHandle, UIListener* listener)
		{
			containedParameters.at(globalHandle - offset).uiListeners.insert(listener);
		}

		/// <summary>
		/// Only safe to call on the UI thread.
		/// </summary>
		void removeUIListener(Parameters::Handle globalHandle, UIListener* listener)
		{
			containedParameters.at(globalHandle - offset).uiListeners.erase(listener);
		}

		/// <summary>
		/// Adds a realtime listeners. See documentation for RTListener. Safe to call from any thread.
		/// If spin is set, the function will always succeed but may spin. May allocate memory.
		/// If not, the return value indicates whether the operation succeeded.
		/// </summary>
		bool addRTListener(RTListener* listener, bool spin = true)
		{
			for (auto& slot : realtimeListeners)
			{
				auto slotListener = slot.listener.load(std::memory_order_acquire);
				if (slotListener == listener)
					return true;
				bool hasLock = false;

				if (!slotListener)
				{

					if (spin)
					{
						while (slot.lock.test_and_set(std::memory_order_relaxed));
						hasLock = true;
					}
					else
					{
						hasLock = !slot.lock.test_and_set(std::memory_order_relaxed);
					}

					if (hasLock)
					{
						slot.listener.store(listener, std::memory_order_release);
						slot.lock.clear(std::memory_order_release);
					}
				}

				if (hasLock)
					return true;

			}
			return false;
		}

		/// <summary>
		/// Safe to call from any thread.
		/// If spin is set, the function will always succeed but may spin.
		/// If not, the return value indicates whether the operation succeeded.
		/// </summary>
		bool removeRTListener(RTListener* listener, bool spin = true)
		{
			for (auto& slot : realtimeListeners)
			{
				auto slotListener = slot.listener.load(std::memory_order_acquire);

				bool hasLock = false;

				if (slotListener == listener)
				{
					if (spin)
					{
						while (slot.lock.test_and_set(std::memory_order_relaxed));
						hasLock = true;
					}
					else
					{
						hasLock = !slot.lock.test_and_set(std::memory_order_relaxed);
					}

					if (hasLock)
					{
						slot.listener.store(nullptr, std::memory_order_release);
						slot.lock.clear(std::memory_order_release);
					}
				}

				if (hasLock)
					return true;

			}
			return false;
		}

		/// <summary>
		/// Safe to call from any thread.
		/// Handle = The local handle.
		/// </summary>
		void updateFromProcessorNormalized(Parameters::Handle globalHandle, T value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
		{
			ParameterView& p = containedParameters.at(globalHandle - offset);

			p.parameter->setValue(value);
			value = p.parameter->getValue();

			if (flags & Parameters::UpdateFlags::RealTimeListeners)
			{
				callRTListenersFor(globalHandle - offset);
			}

			if (flags & Parameters::UpdateFlags::UI)
				p.changedFromProcessor = true;

			if (flags & Parameters::UpdateFlags::RealTimeSubSystem && p.isAutomatable)
			{
				processor.automatedTransmitChangeMessage(globalHandle, static_cast<FrameworkType>(value));
			}
		}

		/// <summary>
		/// Should only be called from a host callback (setParameter)
		/// </summary>
		void updateFromHostNormalized(Parameters::Handle globalHandle, T value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
		{
			ParameterView& p = containedParameters.at(globalHandle - offset);

			p.parameter->setValue(value);

			if (flags & Parameters::UpdateFlags::RealTimeListeners)
			{
				callRTListenersFor(globalHandle - offset);
			}

			if (flags & Parameters::UpdateFlags::UI)
				p.changedFromProcessor = true;

		}

		/// <summary>
		/// Only safe to call on the UI thread.
		/// </summary>
		void updateFromUINormalized(Parameters::Handle globalHandle, T value, Parameters::UpdateFlagsT flags = Parameters::UpdateFlags::All)
		{
			ParameterView& p = containedParameters.at(globalHandle - offset);
			p.parameter->setValue(value);
			value = p.parameter->getValue();

			if (flags & Parameters::UpdateFlags::RealTimeSubSystem && p.isAutomatable)
			{
				processor.automatedTransmitChangeMessage(globalHandle, static_cast<FrameworkType>(value));
			}

			if (flags & Parameters::UpdateFlags::RealTimeListeners)
			{
				callRTListenersFor(globalHandle - offset);
			}

			if (flags & Parameters::UpdateFlags::UI)
			{
				for (auto listener : p.uiListeners)
				{
					listener->parameterChangedUI(globalHandle - offset, globalHandle, &p);
				}
			}

		}

		/// <summary>
		/// Should be called regularly to recieve notifications from the processor
		/// thread
		/// </summary>
		void pulseUI()
		{
			for (std::size_t i = 0; i < containedParameters.size(); ++i)
			{
				if (containedParameters[i].changedFromProcessor.cas())
				{
					for (auto listener : containedParameters[i].uiListeners)
					{
						listener->parameterChangedUI(static_cast<Parameters::Handle>(i), static_cast<Parameters::Handle>(i) + offset, &containedParameters[i]);
					}
				}
			}
		}

		void beginChangeGesture(Parameters::Handle globalHandle)
		{
			processor.automatedBeginChangeGesture(globalHandle);
		}

		void endChangeGesture(Parameters::Handle globalHandle)
		{
			processor.automatedEndChangeGesture(globalHandle);
		}

		/// <summary>
		/// O(N)
		/// </summary>
		Parameters::Handle handleFromName(const std::string_view name) const noexcept
		{
			for (std::size_t i = 0; i < containedParameters.size(); ++i)
			{
				if (name == containedParameters[i].parameter->getName())
				{
					return static_cast<Parameters::Handle>(i) + offset;
				}
			}

			return InvalidHandle;
		}

		/// <summary>
		/// Must only be called from the UI thread
		/// </summary>
		Parameters::Handle mapName(const std::string_view name) noexcept
		{
			auto it = nameMap.find(name);
			if (it == nameMap.end())
			{
				auto handle = handleFromName(name);
				if (handle != InvalidHandle)
				{
					return (nameMap[std::string(name)] = handle) + offset;
				}
			}
			else
			{
				return it->second + offset;
			}
			return InvalidHandle;
		}

		std::size_t size() const noexcept
		{
			return containedParameters.size();
		}

		ParameterView* findParameter(Parameters::Handle globalHandle)
		{
			if (!isSealed)
				CPL_RUNTIME_EXCEPTION("ParameterView being acquired while the system isn't sealed");

			globalHandle -= offset;
			if (globalHandle >= 0 && static_cast<std::size_t>(globalHandle) < containedParameters.size())
			{
				return &containedParameters[globalHandle];
			}
			return nullptr;
		}

		ParameterView* findParameter(const std::string_view name) noexcept
		{
			if (!isSealed)
				CPL_RUNTIME_EXCEPTION("ParameterView being acquired while the system isn't sealed");
			return findParameter(mapName(name));
		}

		const std::string& getName() const noexcept { return groupName; }
		const std::string& getExportPrefix() const noexcept { return prefix; }

		typename std::vector<ParameterView>::iterator begin() { return containedParameters.begin(); }
		typename std::vector<ParameterView>::iterator end() { return containedParameters.end(); }

	private:

		void callRTListenersFor(Parameters::Handle localHandle)
		{
			for (auto & slot : realtimeListeners)
			{
				// acquire is a memory fence in-between listener load and lock load,
				// they must not be reordered
				auto slotListener = slot.listener.load(std::memory_order_acquire);

				if (slotListener)
				{
					// basically a variant of the double-checked lock pattern,
					// that avoids expensive lock operations when not needed
					// acquire-release fence?
					if (!slot.lock.test_and_set(std::memory_order_release))
					{
						// call
						slotListener = slot.listener.load(std::memory_order_acquire);
						if (slotListener)
						{
							slotListener->parameterChangedRT(localHandle, localHandle + offset, containedParameters[localHandle].getParameter());
						}
						slot.lock.clear(std::memory_order_release);
					}
				}

			}
		}

		void onServerDestruction(DestructionNotifier * notif) override
		{
			for (std::size_t i = 0; i < containedParameters.size(); ++i)
			{
				//containedParameters[i].uiListeners.erase(static_cast<UIListener *>(notif));
			}
		}

		bool isSealed;

		struct BundleInstallReference
		{
			Parameters::BundleUpdate<ParameterView> * parent;
			std::vector<typename Parameters::BundleUpdate<ParameterView>::Record> * records;
		};

		struct SingleInstallReference
		{
			Parameters::SingleUpdate<ParameterView> * parent;
			Parameters::CallbackParameterRecord<ParameterView> * record;
		};

		struct RTListenerSlot
		{
			std::atomic_flag lock = ATOMIC_FLAG_INIT;
			std::atomic<RTListener *> listener {nullptr};
		};

		std::unique_ptr<std::vector<BundleInstallReference>> bundleInstalledReferences;
		std::unique_ptr<std::vector<SingleInstallReference>> singleInstalledReferences;
		std::atomic_flag rtListenerLock;
		std::array<RTListenerSlot, 8> realtimeListeners;
		std::unique_ptr<Parameters::UserContent, Utility::MaybeDelete<Parameters::UserContent>> userContent;
		std::string prefix;
		std::string groupName;
		std::map<std::string, Parameters::Handle, std::less<>> nameMap;
		Parameters::Handle offset;
		AutomatedProcessor & processor;
		std::vector<ParameterView> containedParameters;
	};


	class ValueView
	{
		class Listener
		{

		};
	};

};

#endif
