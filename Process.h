/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2017 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	file:Process.h

		Contains a Process class with several utility functions related to invocations,
		arguments and general handling of child-parent communications.

		The process generally runs in the same permission context as the parent, however:
			1) No file handles are inherited, except for optional stdin/out/err
			2) No signal masks are inherited
			3) No fork handlers or anything run
			4) Any std handle not explicitly connected is replaced with a null device (*).

		*) launching the process in a shell gives default std handles to the process (not
		inherited). It is additionally safe to concurrently create processes.

		Includes ability to create std::ostream/std::istreams for the child's
		stdin, stdout and stderr streams.

		This is a small example illustrating the
		simplest form of communication - a parent writing to a child's input,
		and the child immediately writing it back.

		Notice the usage of std::endl, very critical for avoiding deadlocks -
		stdin, stdout and stderr are buffered (as are Process::cin(),
		Process::cout() and Process::cerr()) for performance. std::endl
		ensures a newline is present such that getline() returns,
		but also ensures flushing of the stream.

		Process::cin().close() closes the pipe and eventually causes reading
		operations in the child pname on stdin to fail with eof bit set,
		when there's nothing left in the stream.

		When the child exits, the pipes are broken and Process::cout()
		and Process::cerr() eventually fails with EOF when the streams
		are empty.

		int main_parent()
		{
			using namespace std;

			auto p = cpl::Process::Builder("child").launch("", cpl::Process::in | cpl::Process::out);
			string s;

			while (getline(cin, s))
			{
				if (s == "exit")
					p.cin().close();
				else
					p.cin() << s << endl;

				if (getline(p.cout(), s))
					cout << "response: " + s + "\n";
				else
					break;
			}

			return 0;
		}


		int main_child()
		{
			using namespace std;

			string s;

			while (getline(cin, s))
				cout << s << endl;

			return 0;
		}

*************************************************************************************/

#ifndef CPL_PROCESS_H
#define CPL_PROCESS_H

#include <ostream>
#include <istream>
#include <memory>
#include "MacroConstants.h"
#include "cpl/PlatformSpecific.h"
#include "process/Args.h"
#include "process/Env.h"
#include "process/ProcessUtil.h"
#include <functional>
#include <optional>

namespace cpl
{

	class Process
	{
	public:

		const static std::int64_t npid;

		using InputStream = std::istream;
		using CloseableOutputStream = detail::OutputStream;

		enum IOFlags
		{
			None = 0,
			Out = 1 << 0,
			Err = 1 << 1,
			In = 1 << 2
		};

		using IOStreamFlags = IOFlags;

		/// <summary>
		/// If the process is actual() and alive() upon destruction,
		/// try one of these options in a loop and optionally
		/// call the handler on failure or exceptions.
		/// </summary>
		enum class ScopeExitOperation
		{
			/// <summary> The process will be joined upon destruction (default). </summary>
			Join,
			/// <summary> std::terminate will be called. </summary>
			Terminate,
			/// <summary> The process will be detached upon destruction. </summary>
			Detach,
			/// <summary> The process will be killed, and then joined. </summary>
			KillJoin,
			/// <summary> The process will be killed, and then detached. </summary>
			KillDetach
		};

		typedef std::function<ScopeExitOperation(const Process&, ScopeExitOperation previous, std::exception* e)> ScopeExitHandler;

		class Builder;

		/// <summary>
		/// A default-constructed Process has only to valid operations:
		/// getPid() and actual().
		/// </summary>
		Process() noexcept;
		Process(Process&& other) = default;
		Process(const Process& other) = delete;
		Process& operator = (Process&& other) = default;
		Process& operator = (const Process& other) = delete;
		~Process() noexcept;

		InputStream & cout();
		InputStream & cerr();
		CloseableOutputStream & cin();

		/// <summary>
		/// May return immediately.
		/// </summary>
		/// <remarks>
		/// Throws std::logic_error if not actual().
		/// </remarks>
		const std::string & name() const;
		/// <summary>
		/// May return immediately. join() on the process to synchronize.
		/// </summary>
		/// <remarks>
		///  Throws std::logic_error if not actual().
		/// </remarks>
		std::error_code kill();
		/// <summary>
		/// Closes all connected streams, and orphans off the process
		/// such that the process will never turn into a "zombie"
		/// (for all practical purposes), regardless of when,
		/// if ever, the process exits. After this operation, this object
		/// is as if in a moved or default-constructed state.
		///
		/// Notice that the child process' lifetime may still be connected
		/// to the parent group session. Use Builder::launchDetached() to completely
		/// avoid that situation.
		/// </summary>
		/// <remarks>
		/// Throws std::logic_error if not actual(). Can also throw on grave library runtime errors (like std::bad_alloc)
		/// </remarks>
		void detach();
		/// <summary>
		/// Returns whether this represents a valid process object,
		/// that can be joined or otherwise operated on.
		/// Note that only detach() can mutate this state.
		/// Valid to call on default constructed, moved or
		/// detached() objects.
		/// Effectively is getPid() != npid.
		/// </summary>
		bool actual() const noexcept;
		/// <summary>
		/// A process can be dead ("zombie") but still represent a valid,
		/// pid. No other process system-wide can use this pid while
		/// this object exists.
		/// </summary>
		/// <remarks>
		/// Throws std::logic_error if not actual().
		/// </remarks>
		bool alive();
		/// <summary>
		/// A negative timeout means waiting forever, a timeout
		/// of exactly 0 means checking and returning immediately,
		/// a longer timeout will try joining until the timeout.
		/// If join() returns true, alive() will be mutated to false.
		/// </summary>
		/// <remarks>
		/// Throws std::logic_error if not actual().
		/// </remarks>
		bool join(int timeoutMs = -1);
		/// <summary>
		/// Returns a system-wide unique identifier for this process.
		/// The value is guaranteed to be identical for any native representation,
		/// unless it has the special value of npid, which represents no process.
		/// </summary>
		std::int64_t getPid() const noexcept;
		/// <summary>
		/// Determines what to do in the destructor if actual() is true.
		/// See ScopeExitOperation.
		/// </summary>
		void setScopeExitOperation(ScopeExitOperation newOperation) noexcept;
		/// <summary>
		/// Installs a handler that will be invoked in the destructor if the
		/// scope exit operation failed, or optionally threw an exception.
		/// The handler can optionally do some system-specific behaviour on
		/// the process, and return a new operation to try out in a loop.
		/// Any exception from the handler will result in a call to std::terminate().
		/// </summary>
		void setScopeHandler(ScopeExitHandler newHandler);

		/// <summary>
		/// Returns the exit code of the process.
		/// </summary>
		/// <remarks>
		/// Throws std::logic_error if not actual(), or alive().
		/// </remarks>
		std::int64_t getExitCode();

		static const EnvStrings& getParentEnvironment();
		const EnvStrings& getCreationEnvironment();
		const Args& getCreationArgs();

	private:

		bool doJoin(int timeout);
		#if defined(CPL_UNIXC) && defined(CPL_GCC) && __cplusplus < 201703L
		template<typename T>
		using optional = std::experimental::optional<T>;
		#else
		template<typename T>
		using optional = std::optional<T>;
		#endif
		ScopeExitOperation callHandler(std::exception * e = nullptr) const noexcept;

		class PIDHandle
		{
		public:
			std::int64_t get() const noexcept { return value; }
			void set(std::int64_t newValue) noexcept { value = newValue; }
			PIDHandle(std::int64_t val) noexcept : value(val) {}
			PIDHandle& operator = (PIDHandle && other) noexcept { value = other.value; other.value = npid; return *this; }
			PIDHandle(PIDHandle && other) noexcept : value(other.value) { other.value = npid; }
			PIDHandle(const PIDHandle& other) = delete;
			PIDHandle& operator = (const PIDHandle& other) = delete;
		private:
			std::int64_t value;
		};

		static EnvStrings GetEnvironment();
		const static EnvStrings initialEnvironment;

		typedef std::pair<detail::unique_handle, detail::unique_handle> PipePair;

		void initialise(PipePair& in, PipePair& out, PipePair& err, EnvStrings * env, const std::string * cwd, int customFlags);
		void releaseSpecific() noexcept;
		static PipePair createPipe(int parentEnd = -1);
		Process(std::string process, Args&& args, int ioFlags, ScopeExitOperation operation, const EnvStrings * env, const std::string * cwd, int customFlags = 0);

		PIDHandle pid;
		IOStreamFlags flags;
		std::string pname;
		Args pArgs;
		EnvStrings pEnv;
		ScopeExitOperation scopeExitOp;
		ScopeExitHandler handler;
		optional<std::int64_t> exitCode;
		bool explicitlyJoined;
		bool hasCustomEnvironment;

		typedef detail::FileEdge<detail::InputBuffer, std::istream> InPipe;
		typedef detail::FileEdge<detail::OutputBuffer, detail::OutputStream> OutPipe;

		#ifdef CPL_WINDOWS
		detail::unique_handle
			childProcessHandle,
			childThreadHandle;
		#endif

		std::unique_ptr<OutPipe> pin;
		std::unique_ptr<InPipe> pout, perr;
	};


	class Process::Builder
	{
	public:

		Builder(Builder&& other) = default;
		Builder& operator = (Builder&& other) = default;

		Builder(std::string processLocation)
			: process(std::move(processLocation))
		{

		}

		Builder& workingDir(std::string newWorkingDir)
		{
			hasCWD = true;
			cwd = std::move(newWorkingDir);
			return *this;
		}

		Builder& environment(const EnvStrings& newEnv)
		{
			hasEnv = true;
			env = newEnv;
			return *this;
		}

		/// <summary>
		/// Note that it is undefined behaviour to launch an executable that might disconnect
		/// itself from the parent. Use launchDetached for that.
		/// </summary>
		Process launch(Args args = Args(), int ioFlags = IOStreamFlags::None, ScopeExitOperation operation = ScopeExitOperation::Join)
		{
			return {
				process,
				std::move(args),
				ioFlags,
				operation,
				hasEnv ? &env : nullptr,
				hasCWD ? &cwd : nullptr
			};
		}

		/// <summary>
		/// Launch a process completely detached from the parent.
		/// </summary>
		void launchDetached(Args args = Args());

		/// <summary>
		/// Invokes the system command interpreter, and runs the command line.
		/// Note the process represents the command interpreter, not the child
		/// process. It is implementation-defined whether the exit code from the child
		/// is propagated.
		/// </summary>
		Process shell(Args args = Args(), int ioFlags = IOStreamFlags::None, ScopeExitOperation operation = ScopeExitOperation::Join);

		/// <summary>
		/// Invokes the default terminal UI, and runs the program inside it.
		/// Note that the returned process represents a wrapper parent process,
		/// and not the child process. It is implementation-defined whether the exit
		/// code from the child is propagated.
		/// </summary>
		Process terminal(Args args = Args(), ScopeExitOperation operation = ScopeExitOperation::Join);

	private:
		std::string process;
		std::string cwd;
		EnvStrings env;
		bool hasCWD = false, hasEnv = false;
	};
}

#endif
