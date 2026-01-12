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

	file:CSerializer.h

		A class that works like a stream, where you can write any POD type into,
		including itself, or any type deriving from CSerializer::Serializable.
		CSerializers carry version info and has an associated key.
		Keys are either integers or strings, and are compatible (and comparable).
		This means that CSerializers has binary data, but also has an associative binary tree
		map of itself.

		This class is designed to serialize objects to disk / memory, and restore them perfectly
		again.

	Example usage:

		class MyObject : public CSerializer::Serializable
		{
			struct data_t
			{
				int something;
				float somethingElse;
			} data;
			std::string text;
			MyObject * child;

			void save(CSerializer::Archiver & archive, Version version) {

				archive << data;
				archive << text;
				if(child)
				{
					// will recursively save any childs.
					archive.getContent("child") << *child;
				}
			}

			void load(CSerializer::Builder & builder, Version version) {

				if(version != supported_version)
					return;

				builder >> data;
				builder >> text;
				auto & entry = builder.getContent("child");

				if(!entry.isEmpty())
				{
					// will recursively load any childs.
					child = new Child;
					entry >> *child;
				}

			}
		};


		void somethingThatSavesState()
		{
			CSerializer se;
			se.setMasterVersion(this_program_version);

			myobject.save(se, se.getLocalVersion());
			auto data = se.compile();

			writeToFile(data.getBlock(), data.getSize());
		}

		void somethingThatReadsState()
		{
			CSerializer se;

			auto data = readAFile();
			auto dataSize = fileSize(data);
			// this may throw an exception if file is corrupt.
			se.build(CSerializer::WeakContentWrapper(data, dataSize));

			myobject.load(se, se.getLocalVersion());

		}

		A class semantically equivalent to boost::archive, except this writes data as
		binary, and can write/load itself as well.

*************************************************************************************/

#ifndef CPL_CSERIALIZER_H
#define CPL_CSERIALIZER_H

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <exception>
#include <stdexcept>
#include <atomic>
#include <optional>
#include "../stdext.h"
#include "../Misc.h"
#include "../ProgramVersion.h"
#include "../Exceptions.h"
#include "../lib/weak_atomic.h"

namespace cpl
{

	class BinaryBuilder
	{
	public:
		typedef char basic_element;
		typedef basic_element byte;

		template<typename org_type>
		struct custom_deleter
		{
			template<typename new_type>
			void operator()(new_type * obj)
			{
				auto casted_ptr = reinterpret_cast<org_type*>(obj);
				delete[] casted_ptr;
			}
		};

		using deleter = custom_deleter < basic_element >;

		template<typename T>
		using owned_ptr = std::unique_ptr <T, deleter >;

		BinaryBuilder()
			: memory(nullptr), bytesUsed(0), bytesAllocated(0), readPtr(0)
		{

		}
		// the 'big' five
		BinaryBuilder(const BinaryBuilder & other)
			: memory(nullptr), bytesUsed(0), bytesAllocated(0), readPtr(0)
		{
			appendBytes(other.getPointer(), other.getSize());
			readPtr = other.readPtr;
		}

		BinaryBuilder(BinaryBuilder && other) noexcept
			: memory(nullptr), bytesUsed(0), bytesAllocated(0), readPtr(0)
		{
			bytesAllocated = other.bytesAllocated;
			bytesUsed = other.bytesUsed;
			readPtr = other.readPtr;
			memory = std::move(other.memory);
		}

		BinaryBuilder & operator = (const BinaryBuilder & other)
		{
			reset();
			appendBytes(other.getPointer(), other.getSize());
			readPtr = other.readPtr;
			return *this;
		}

		BinaryBuilder & operator = (BinaryBuilder && other) noexcept
		{
			reset();
			bytesAllocated = other.bytesAllocated;
			bytesUsed = other.bytesUsed;
			readPtr = other.readPtr;
			memory = std::move(other.memory);
			return *this;
		}

		void appendBytes(const void * content, std::uint64_t bytesl)
		{
			if (!bytesl)
				return;

			std::size_t bytes = static_cast<std::size_t>(bytesl);
			ensureExtraBytes(bytes);
			std::memcpy(getCurrentPointer(), content, bytes);
			bytesUsed += bytes;
			// apply this for FIFO
			//readPtr = bytesUsed;
		}

		std::string getString()
		{
			std::size_t size = 0;
			if (!bytesUsed || !getPointer())
				return "";

			const char * pointer = reinterpret_cast<const char *>(memory.get());
			pointer += readPtr;
			for (;;)
			{
				// no nulltermination, assume something is corrupt and bail out
				if (size > bytesUsed)
					return "";
				if (pointer[size] == '\0')
					break;
				size++;

			}
			// add one more byte to include the nullterminator
			readPtr += size + 1;
			if (size > 0)
			{
				// notice it reads size bytes from pointer, not
				// [pointer .. pointer + size] which would be
				// one more byte, thus including the zero terminator.
				return std::string(pointer, size);
			}
			else
				return "";
		}

		bool readBytes(void * content, std::size_t bytes)
		{
			// bad params
			if (!bytes || !content)
				return false;
			// reading more than what's available
			if (bytes + readPtr > bytesUsed)
				return false;
			// nothing to read - or reading more than what's available
			if (!getPointer())
				return false;

			std::memcpy(content, getPointer() + readPtr, bytes);
			readPtr += bytes;
			return true;
		}

		void ensureExtraBytes(std::size_t ebytes)
		{
			return ensureBytes(ebytes + bytesUsed);
		}

		void ensureBytes(std::size_t bytes)
		{
			if (bytes > bytesAllocated)
			{
				std::size_t newSize = static_cast<std::size_t>(bytes * 1.5);
				auto newPtr = std::make_unique<char[]>(newSize);
				std::memcpy(newPtr.get(), memory.get(), bytesUsed);
				bytesAllocated = newSize;
				memory.reset(newPtr.release());
			}
		}
		char * getCurrentPointer() const { return memory.get() + bytesUsed; }
		char * getPointer() const { return memory.get(); }
		std::size_t getSize() const { return bytesUsed; }
		void reset() { acquirePointer(); }
		void rewindRead() { readPtr = 0; }
		void rewindWrite() { bytesUsed = 0; }
		owned_ptr<byte[]> acquirePointer() { readPtr = bytesAllocated = bytesUsed = 0; return owned_ptr<byte[]>(memory.release()); }
	private:

		std::size_t bytesAllocated, bytesUsed;
		std::size_t readPtr;
		owned_ptr<byte[]> memory;
	};

	class ContentWrapper
	{
	public:
		//ContentWrapper(std::string filePath);

		ContentWrapper(BinaryBuilder & b)
		{
			contentSize = b.getSize();
			contents.reset(b.acquirePointer().release());
		}

		ContentWrapper(BinaryBuilder::owned_ptr<BinaryBuilder::byte[]> memory, std::size_t size)
			: contents(memory.release()), contentSize(size)
		{

		}

		ContentWrapper(ContentWrapper&& cw) noexcept
			: contents(cw.contents.release()), contentSize(cw.contentSize)
		{

		}

		const char * getBlock() const { return static_cast<const char*>(contents.get()); }
		std::size_t getSize() const { return contentSize; }

	private:
		std::unique_ptr<char[]> contents;
		std::size_t contentSize;
	};

	class WeakContentWrapper
	{
	public:
		WeakContentWrapper(const BinaryBuilder::byte * c, std::uint64_t size) : contents(c), contentSize(size) {}
		WeakContentWrapper(const void * c, std::uint64_t size)
			: contents(static_cast<const BinaryBuilder::byte *>(c)), contentSize(size) {}
		WeakContentWrapper(const ContentWrapper & cw) : contents(cw.getBlock()), contentSize(cw.getSize()) {}

		const char * getBlock() const { return contents; }
		std::uint64_t getSize() const { return contentSize; }

	private:
		const BinaryBuilder::byte * contents;
		std::uint64_t contentSize;
	};

	class ISerializerSystem
	{
	public:
		virtual bool build(const WeakContentWrapper & cr) = 0;
		virtual ContentWrapper compile(bool addMasterHeader = false) const = 0;
		virtual void clear() = 0;
		virtual bool isEmpty() const noexcept = 0;
		virtual ~ISerializerSystem() {};
	};

	class CSerializer : public ISerializerSystem
	{
	public:

		enum class Modifiers
		{
			/// <summary>
			/// Stack-modified. If set (internally), all writes & reads modify read ptrs, but do not
			/// store or read data; any references passed in are unmodified.
			/// </summary>
			Virtual,
			RestoreValue,
			RestoreSettings
		};

		typedef CSerializer Archiver;
		typedef CSerializer Builder;

		class ExhaustedException : public CPLRuntimeException
		{
			using CPLRuntimeException::CPLRuntimeException;
		};

		class Serializable
		{
		public:
			virtual ~Serializable() {};

			/// <summary>
			/// Serialize the object state to the archiver, in a version
			/// compatible with the argument version
			/// </summary>
			virtual void serialize(Archiver & ar, Version version) {};
			/// <summary>
			/// Deserialize the content to your object. The content comes from
			/// version argument version.
			/// </summary>
			virtual void deserialize(Builder & ar, Version version) {};
		};

		enum class DeprecatedBinaryDeserialization {};

		template<typename T>
		class OptionalWrapper : public Serializable
		{
			typedef typename std::aligned_storage<sizeof(std::optional<T>), alignof(std::optional<T>)>::type binary_option_type;
			typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type binary_type;

		public:

			static_assert(std::is_standard_layout_v<T> && !std::is_pointer_v<T> && !std::is_array_v<T>, "Cannot serialize such objects");

			OptionalWrapper(std::optional<T>& option) : option(option), deserializeBinary(false) {}
			/// <summary>
			/// Use this tag constructor if you've previously serialized optionals directly, which isn't supported going forward.
			/// </summary>
			OptionalWrapper(std::optional<T>& option, DeprecatedBinaryDeserialization _) : option(option), deserializeBinary(true) {}

			void serialize(Archiver& ar, Version version) override
			{
				std::int8_t shouldHaveValue = option.has_value();

				ar << shouldHaveValue;

				if (shouldHaveValue)
				{
					ar << *option;
				}
				else
				{
					binary_type data{};
					std::memset(&data, 0, sizeof(data));

					ar << data;
				}
			}

			void deserialize(Builder& builder, Version version) override
			{
				if (!deserializeBinary)
				{
					binary_type data{};
					std::memset(&data, 0, sizeof(data));

					std::int8_t shouldHaveValue;
					builder >> shouldHaveValue;

					if (shouldHaveValue)
					{
						if (option.has_value())
						{
							builder >> *option;
						}
						else
						{
							builder >> data;
							// To deserialize optionals, please provide a (void*, size_t) constructor
							static_assert(sizeof(data) == sizeof(T), "Mismatched types");
							option = T(&data, sizeof(T));
						}
					}
					else
					{
						// just throwaway
						builder >> data;
						option = {};
					}
				}
				else
				{
					binary_option_type data{};

					builder >> data;
					static_assert(sizeof(typename std::remove_cv<decltype(option)>::type) == sizeof(binary_option_type), "Mismatched option types");
					std::memcpy(&option, &data, sizeof(data));

					return;
				}
			}

		private:
			OptionalWrapper() = delete;
			OptionalWrapper(const OptionalWrapper&) = delete;

			std::optional<T>& option;
			bool deserializeBinary;
		};

		enum class HeaderType : std::uint16_t
		{
			Start = 0x10, // some sentinel value makes debugging easier
			Key,
			Data,
			Child,
			End,
			CheckedHeader,
			LocalVersion,
			Invalid // always add new types before invalid, such that old programs identify new serializable data as invalid
		};
		template<typename ExtraHeader>
		struct CPL_ALIGNAS(8) BinaryHeader
		{
			std::uint64_t headerSize;
			std::uint64_t dataSize;
			HeaderType type;
			ExtraHeader info;

			BinaryHeader()
				: headerSize(sizeof(*this)), dataSize(0), type(HeaderType::Invalid), info()
			{}
			const BinaryHeader<int> * next() const
			{
				if (type == HeaderType::End)
					return nullptr;
				else
					return (BinaryHeader<int>*)(((char*)this) + headerSize + dataSize);
			}
			template<typename T>
			const T * getData() const
			{
				return reinterpret_cast<const T *>(((const char *)this) + sizeof(*this));
			}
		};

		struct MasterHeaderInfo
		{
			std::uint64_t totalSize;
			Version::BinaryStorage versionID;
		};

		struct KeyHeaderInfo
		{
			bool isString;
			std::int64_t ID;
		};

		struct DataHeaderInfo
		{

		};

		struct LocalVersionInfo
		{
			Version::BinaryStorage version;
		};

		using MasterHeader = BinaryHeader < MasterHeaderInfo >;
		using KeyHeader = BinaryHeader < KeyHeaderInfo >;
		using StdHeader = BinaryHeader < int >;
		using MD5CheckedHeader = BinaryHeader <uint8_t[16]>;
		using LocalVersionHeader = BinaryHeader <LocalVersionInfo>;

		class Key
		{
		public:
			explicit Key(const KeyHeader* kh) = delete;
			Key(const std::string & s) : isString(true), intKey(0), stringKey(s) {};
			Key(const char * s) : isString(true), intKey(0), stringKey(s) {};
			Key(long long ID) : isString(false), intKey(ID) {};

			bool operator < (const Key & other) const
			{
				// true return == other > this

				// strings compare greater than IDs
				if (other.isString)
				{
					if (!isString)
						return true;
					else
						return stringKey < other.stringKey;
				}
				else
				{
					if (isString)
						return false;
					else
						return intKey < other.intKey;
				}
				return false;
			}

			bool operator == (const Key & other) const
			{
				if (other.isString != isString)
					return false;

				if (other.isString)
				{
					if (other.stringKey == stringKey)
						return true;
				}
				else
				{
					if (other.intKey == intKey)
						return true;
				}
				return false;
			}

			// compiles this state down to a block of bytes.
			// build does the reverse action.
			// the data inside can be cast to a const KeyHeader *
			ContentWrapper compile() const
			{
				BinaryBuilder compiledData;
				KeyHeader header;
				// we dont carry the additional zero-terminating null, since the size is already given.
				header.dataSize = isString ? stringKey.length() : 0;
				header.type = HeaderType::Key;
				header.info.isString = isString;
				header.info.ID = intKey;

				compiledData.appendBytes(&header, sizeof(header));
				if (isString)
					compiledData.appendBytes(stringKey.c_str(), stringKey.size());

				return ContentWrapper(compiledData);
			}

			// restores this object to the state found in kh.
			bool build(const KeyHeader * kh)
			{
				if (kh->headerSize >= sizeof(KeyHeader))
				{
					isString = kh->info.isString;
					intKey = kh->info.ID;
					if (isString && kh->dataSize > 0)
					{
						stringKey = std::string(kh->getData<char>(), kh->getData<char>() + kh->dataSize);
					}
					return true;
				}
				else
					return false;
			}

		private:
			bool isString;
			std::string stringKey;
			long long intKey;
		};


		CSerializer(Key k = Key(1), Version v = Version())
			: key(k)
			, version(v)
			, throwOnExhaustion(true)
			, virtualCount(0)
			, restoreSettings(true)
			, restoreValue(true)
		{

		}

		void modify(Modifiers m, bool toggle = true)
		{
			switch (m)
			{
				case Modifiers::Virtual: virtualCount += toggle ? 1 : -1; break;
				case Modifiers::RestoreSettings: restoreSettings = !!toggle; break;
				case Modifiers::RestoreValue: restoreValue = !!toggle; break;
			}

			if (virtualCount < 0)
				CPL_RUNTIME_EXCEPTION("Virtual count modified to below zero; mismatch");
		}

		bool getModifier(Modifiers m)
		{
			switch (m)
			{
				case Modifiers::Virtual: return virtualCount > 0;
				case Modifiers::RestoreSettings: return restoreSettings;
				case Modifiers::RestoreValue: return restoreValue;
				default:
					return false;
			}

		}

		~CSerializer()
		{
			clear();
		}

		/// <summary>
		/// Clears all content and children in this object, resetting it's state.
		/// Note: Does currently NOT reset the local version.
		/// </summary>
		void clear() override
		{
			content.clear();
			data.reset();
		}

		void setThrowsOnExhaustion(bool toggle) noexcept
		{
			throwOnExhaustion = toggle;
		}

		bool getThrowsOnExhaustion() const noexcept
		{
			return throwOnExhaustion;
		}

		bool isEmpty() const noexcept override
		{
			return !content.size() && !data.getSize();
		}
		void clearDataOnly()
		{
			for (auto & child : content)
			{
				child.second.clearDataOnly();
			}
			data.reset();
		}

		/// <summary>
		/// Sets the master version for all subsequent operations. Notice this has an recursive
		/// effect, unlike any load/save calls with explicit parameters, that are local instead.
		/// </summary>
		/// <param name="v"></param>
		/// <returns></returns>
		void setMasterVersion(Version v) noexcept { version = v; }
		Version getLocalVersion() const noexcept { return version; }
		virtual bool build(const WeakContentWrapper & cr) override;
		virtual ContentWrapper compile(bool addMasterHeader = false) const override;

		void append(const CSerializer & se);
		void append(const Key & k);

		template<typename T>
		typename std::enable_if<std::is_pod<T>::value, void>::type
			append(T * object, std::size_t size = SIZE_MAX)
		{
			if (size == SIZE_MAX)
			{
				size = sizeof(T);
			}

			if (virtualCount > 0)
				fill(size);
			else
				data.appendBytes(object, size);
		}

		/// <summary>
		/// WARNING - if you serialize your OWN objects and the serializer is in BINARY mode,
		/// please only use verifiable fixed-size objects (like std::uint64_t), IFF you want to
		/// stay architechture-independant!
		/// </summary>
		template<typename T>
		typename std::enable_if<std::is_standard_layout<T>::value && !std::is_pointer<T>::value && !std::is_array<T>::value, CSerializer &>::type
			operator << (const T & object)
		{
			if (virtualCount > 0)
				fill(sizeof(T));
			else
				data.appendBytes(&object, sizeof(T));

			return *this;
		}

		/// <summary>
		/// Advances the read pointer by N.
		/// </summary>
		bool discard(std::size_t bytes)
		{
			unsigned char c = 0;
			bool res = true;
			for (std::size_t i = 0; res && i < bytes; ++i)
				res = res && data.readBytes(&c, 1);

			return res;
		}

		/// <summary>
		/// Fills in N bytes, intended to be discarded()
		/// </summary>
		void fill(std::size_t bytes)
		{
			unsigned char c = 0;

			for (std::size_t i = 0; i < bytes; ++i)
				data.appendBytes(&c, 1);
		}

		void rewindReader()
		{
			data.rewindRead();
			for (auto & s : content)
				s.second.rewindReader();

		}
		void rewindWriter()
		{
			data.rewindWrite();
			for (auto & s : content)
				s.second.rewindWriter();
		}

		template<typename T, typename D>
		CSerializer & operator << (const std::unique_ptr<T, D> & object)
		{
			static_assert(delayed_error<T>::value, "Serialization of std::unique_ptr is disabled (it is most likely NOT what you want; otherwise use .get())");
		}

		template<typename T, typename D>
		CSerializer & operator >> (std::unique_ptr<T, D> & object)
		{
			static_assert(delayed_error<T>::value, "Deserialization of std::unique_ptr is disabled (it is most likely NOT what you want; otherwise use .get())");
		}

		template<typename T>
		CSerializer& operator << (const std::shared_ptr<T>& object)
		{
			static_assert(delayed_error<T>::value, "Serialization of std::shared_ptr is disabled (it is most likely NOT what you want; otherwise use .get())");
		}

		template<typename T>
		CSerializer& operator >> (std::shared_ptr<T>& object)
		{
			static_assert(delayed_error<T>::value, "Deserialization of std::shared_ptr is disabled (it is most likely NOT what you want; otherwise use .get())");
		}

		template<typename T>
		CSerializer& operator >> (std::optional<T>& object)
		{
			static_assert(delayed_error<T>::value, "Serialization of std::optional is disabled (don't count on it being binary stable)");
		}

		template<typename T>
		CSerializer& operator << (std::optional<T>& object)
		{
			static_assert(delayed_error<T>::value, "Deserialization of std::optional is disabled (don't count on it being binary stable)");
		}

		/// <summary>
		/// WARNING - if you serialize your OWN objects and the serializer is in BINARY mode,
		/// please only use verifiable fixed-size objects (like std::uint64_t), IFF you want to
		/// stay architecture-independent!
		/// </summary>
		template<typename T>
		typename std::enable_if<std::is_standard_layout<T>::value && !std::is_pointer<T>::value && !std::is_array<T>::value, CSerializer &>::type
			operator >> (T & object)
		{
			bool result = false;

			if (virtualCount > 0)
			{
				result = discard(sizeof(T));
			}
			else
			{
				result = data.readBytes(&object, sizeof(T));
			}

			if (!result && throwOnExhaustion)
				CPL_RUNTIME_EXCEPTION_SPECIFIC("CSerializer exhausted; probably incompatible serialized object.", ExhaustedException);

			return *this;
		}

		template<typename T>
		CSerializer& operator >> (std::atomic<T> & object)
		{
			T temp;
			(*this) >> temp;
			object.store(temp, std::memory_order_release);
			return *this;
		}

		template<typename T>
		CSerializer& operator << (const std::atomic<T> & object)
		{
			return (*this) << object.load(std::memory_order_acquire);
		}

		template<typename T>
		CSerializer& operator >> (weak_atomic<T>& object)
		{
			T temp;
			(*this) >> temp;
			object = temp;
			return *this;
		}

		template<typename T>
		CSerializer& operator >> (relaxed_atomic<T>& object)
		{
			T temp;
			(*this) >> temp;
			object = temp;
			return *this;
		}

		template<typename T>
		CSerializer& operator << (const weak_atomic<T>& object)
		{
			return (*this) << object.load();
		}

		template<typename T>
		CSerializer& operator << (const relaxed_atomic<T>& object)
		{
			return (*this) << object.load();
		}

		inline CSerializer & operator >> (Serializable * object)
		{
			object->deserialize(*this, version);
			return *this;
		}

		inline CSerializer & operator << (Serializable * object)
		{
			object->serialize(*this, version);
			return *this;
		}

		inline CSerializer & operator >> (Serializable & object)
		{
			object.deserialize(*this, version);
			return *this;
		}

		inline CSerializer & operator << (Serializable & object)
		{
			object.serialize(*this, version);
			return *this;
		}
		
		inline CSerializer & operator >> (Serializable && object)
		{
			object.deserialize(*this, version);
			return *this;
		}

		inline CSerializer & operator << (Serializable && object)
		{
			object.serialize(*this, version);
			return *this;
		}
        
		CSerializer & operator << (const std::string& str)
		{
			if (virtualCount > 0)
				fill(str.size() + 1);
			else
				data.appendBytes(str.c_str(), str.size() + 1);
			return *this;
		}

		CSerializer & operator << (const string_ref str)
		{
			if (virtualCount > 0)
				fill(str.size() + 1);
			else
				data.appendBytes(str.c_str(), str.size() + 1);
			return *this;
		}
        
		CSerializer & operator >> (std::string & str)
		{
			if (virtualCount > 0)
				data.getString();
			else
				str = data.getString();

			return *this;
		}

		CSerializer & getContent(const Key & k)
		{
			auto it = content.find(k);

			if (it == content.end())
			{
				auto && child = content.emplace(k, k);
				child.first->second.setMasterVersion(version);
				return child.first->second;
			}
			return it->second;
		}

		CSerializer & operator[] (const Key & k)
		{
			return getContent(k);
		}

		const CSerializer * findForKey(const Key & k) const noexcept
		{
			const CSerializer * ret = nullptr;
			auto it = content.find(k);
			if (it != content.end())
				ret = &it->second;

			return ret;
		}

	private:

		BinaryBuilder data;
		//std::vector<std::pair<Key, CSerializer>> content;
		std::map<Key, CSerializer> content;
		Key key;
		bool throwOnExhaustion;
		Version version;
		int virtualCount;
		bool restoreSettings, restoreValue;
		// iterator interface. must be placed after declaration in MSVC
	public:
		auto begin() const noexcept -> decltype(content.begin())
		{
			return content.begin();
		}

		auto end() const noexcept -> decltype(content.end())
		{
			return content.end();
		}
	};

	class CCheckedSerializer : public ISerializerSystem
	{
	public:
		CCheckedSerializer(std::string uniqueNameReference)
			: nameReference(std::move(uniqueNameReference))
		{
			if (!nameReference.size())
				CPL_RUNTIME_EXCEPTION("CheckedSerializer needs to have a non-null name!");
		}


		virtual ContentWrapper compile(bool addMasterHeader = false) const override;

		virtual bool build(const WeakContentWrapper & cr) override;

		void clear() override
		{
			internalSerializer.clear();
		}

		bool isEmpty() const noexcept override
		{
			if (auto content = internalSerializer.findForKey("Content"))
				return content->isEmpty();
			return true;
		}

		CSerializer::Archiver & getArchiver()
		{
			return internalSerializer.getContent("Content");
		}

		CSerializer::Builder & getBuilder()
		{
			return internalSerializer.getContent("Content");
		}

		const std::string& getName() const { return nameReference; }

	private:

		CSerializer internalSerializer;
		std::string nameReference;
	};

};
#endif
