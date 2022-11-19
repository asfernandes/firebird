#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/jrd.h"
#include "../jrd/EngineInterface.h"
#include "../common/StatusHolder.h"
#include "../common/classes/auto.h"
#include "../common/classes/RefCounted.h"
#include "../jrd/lck_proto.h"
#include <thread>

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(LockSuite)


BOOST_AUTO_TEST_SUITE(LockTests)

BOOST_AUTO_TEST_CASE(LockTest)
{
	struct ThreadInfo
	{
		unsigned threadNum;
		Lock* lock;
	};

	const auto blockingAst = [](void* astObject) -> int
	{
		auto threadInfo = (ThreadInfo*) astObject;

		try
		{
			const auto dbb = threadInfo->lock->lck_dbb;
			AsyncContextHolder tdbb(dbb, FB_FUNCTION, threadInfo->lock);

			LCK_release(tdbb, threadInfo->lock);
		}
		catch (const Exception&)
		{
		}

		return 0;
	};

	const auto filename = "/tmp/test1.fdb";

	LocalStatus initLocalStatus;
	CheckStatusWrapper initStatusWrapper(&initLocalStatus);

	RefPtr<JProvider> provider(FB_NEW JProvider(nullptr));

	auto initAttachment = makeNoIncRef(provider->createDatabase(&initStatusWrapper, filename, 0, nullptr));
	initLocalStatus.check();

	std::vector<std::thread> threads;
	std::atomic_int counter(0);
	constexpr unsigned THREAD_COUNT = 128;
	constexpr unsigned ITER_COUNT = 2048;

	for (unsigned threadNum = 0; threadNum < THREAD_COUNT; ++threadNum)
	{
		threads.push_back(std::thread([&, threadNum]() {
			LocalStatus localStatus;
			CheckStatusWrapper statusWrapper(&localStatus);

			AutoPtr<Lock> lock;	// 1!
			ThreadInfo threadInfo;

			auto attachment = makeNoIncRef(provider->attachDatabase(&statusWrapper, filename, 0, nullptr));
			localStatus.check();

			{	// scope
				threadInfo.threadNum = threadNum;

				const auto level = threadInfo.threadNum % 2 == 0 ? LCK_EX : LCK_SR;

				EngineContextHolder tdbb(&statusWrapper, attachment.getPtr(), FB_FUNCTION);

				///AutoPtr<Lock> lock;	// 2!

				lock.reset(FB_NEW_RPT(attachment->getPool(), 0) Lock(tdbb, 0,
					LCK_test_attachment, &threadInfo, blockingAst));

				threadInfo.lock = lock.get();

				for (unsigned i = 0; i < ITER_COUNT; ++i)
				{
					if (level == LCK_SR)
					{
						if (lock->lck_logical == LCK_none)
							LCK_lock(tdbb, lock, level, LCK_WAIT);
					}
					else
					{
						auto locked = LCK_lock(tdbb, lock, level, LCK_WAIT);
						if (locked)
							LCK_release(tdbb, lock);
					}

					++counter;

					EngineCheckout checkout(tdbb, __FUNCTION__);
				}

				if (lock->lck_logical != LCK_none)
					LCK_release(tdbb, lock);
			}

			attachment->detach(&statusWrapper);
			localStatus.check();
		}));
	}

	for (auto& thread : threads)
		thread.join();

	BOOST_CHECK(counter == THREAD_COUNT * ITER_COUNT);

	initAttachment->dropDatabase(&initStatusWrapper);
	initLocalStatus.check();

	initAttachment = nullptr;

	provider->shutdown(&initStatusWrapper, 0, fb_shutrsn_app_stopped);
	initLocalStatus.check();
}

BOOST_AUTO_TEST_SUITE_END()	// LockTests


BOOST_AUTO_TEST_SUITE_END()	// LockSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
