#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/jrd.h"
#include "../jrd/EngineInterface.h"
#include "../common/StatusHolder.h"
#include "../common/classes/auto.h"
#include "../common/classes/RefCounted.h"
#include "../jrd/lck_proto.h"
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace Firebird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(LockSuite)


BOOST_AUTO_TEST_SUITE(LockTests)

BOOST_AUTO_TEST_CASE(DeadLockTest)
{
	struct Workload
	{
		StatusHolder localStatus;
		CheckStatusWrapper statusWrapper{&localStatus};
		RefPtr<JAttachment> attachment;
		AutoPtr<Lock> lock;
		unsigned threadNum;
		unsigned level;
	};

	const auto blockingAst = [](void* astObject) -> int
	{
		auto workload = (Workload*) astObject;

		try
		{
			const auto dbb = workload->lock->lck_dbb;
			AsyncContextHolder tdbb(dbb, FB_FUNCTION, workload->lock);

			printf("thread %u (%u) - level %u - before LCK_release\n", workload->threadNum, (unsigned) gettid(), workload->level);
			LCK_release(tdbb, workload->lock);
			printf("thread %u (%u) - level %u - after LCK_release\n", workload->threadNum, (unsigned) gettid(), workload->level);
		}
		catch (const Exception&)
		{
			printf("thread %u (%u) - level %u - AST exception\n", workload->threadNum, (unsigned) gettid(), workload->level);
		}

		return 0;
	};

	const auto filename = "/tmp/test1.fdb";

	LocalStatus initLocalStatus;
	CheckStatusWrapper initStatusWrapper(&initLocalStatus);

	AutoPlugin<JProvider> provider(JProvider::getInstance());

	auto initAttachment = makeNoIncRef(provider->createDatabase(&initStatusWrapper, filename, 0, nullptr));
	initLocalStatus.check();

	std::vector<Workload*> workloads;
	std::vector<std::thread> threads;

	std::mutex initMtx;
	std::condition_variable initCondVar;
	unsigned initCount = 0;

	constexpr unsigned THREAD_COUNT = 4;

	for (unsigned threadNum = 0; threadNum < THREAD_COUNT; ++threadNum)
	{
		const auto workload = new Workload();
		workload->attachment = makeNoIncRef(provider->attachDatabase(&workload->statusWrapper, filename, 0, nullptr));
		workload->localStatus.check();
		workload->threadNum = threadNum;
		workload->level = threadNum == 2 ? LCK_SR : LCK_EX;
		workloads.push_back(workload);

		threads.push_back(std::thread([&, workload]() {
			{	// scope
				EngineContextHolder tdbb(&workload->statusWrapper, workload->attachment.getPtr(), FB_FUNCTION);

				{	// scope
					std::unique_lock initMtxGuard(initMtx);
					++initCount;

					printf("thread %u (%u) - level %u - before wait\n", workload->threadNum, (unsigned) gettid(), workload->level);
					initCondVar.wait(initMtxGuard, [&] { return initCount == THREAD_COUNT; });
					printf("thread %u (%u) - level %u - after wait\n", workload->threadNum, (unsigned) gettid(), workload->level);
				}

				initCondVar.notify_all();

				workload->lock.reset(FB_NEW_RPT(workload->attachment->getPool(), 0) Lock(tdbb, 0,
					LCK_test_attachment, workload, blockingAst));

				if (workload->lock->lck_logical == LCK_none)
				{
					printf("thread %u (%u) - level %u - before LCK_lock\n", workload->threadNum, (unsigned) gettid(), workload->level);
					LCK_lock(tdbb, workload->lock, workload->level, LCK_WAIT);
					printf("thread %u (%u) - level %u - after LCK_lock\n", workload->threadNum, (unsigned) gettid(), workload->level);
				}
				else
				{
					printf("thread %u (%u) - level %u - was locked\n", workload->threadNum, (unsigned) gettid(), workload->level);
				}
			}

			printf("thread %u (%u) - level %u - checked out\n", workload->threadNum, (unsigned) gettid(), workload->level);
		}));
	}

	{	// scope
		unsigned workloadNum = 0;

		for (auto& thread : threads)
		{
			auto workload = workloads[workloadNum];
			printf("thread %u (%u) - level %u - joinning\n", workload->threadNum, (unsigned) gettid(), workload->level);
			thread.join();

			++workloadNum;
		}
	}

	for (auto workload : workloads)
	{
		printf("thread %u (%u) - level %u - detach\n", workload->threadNum, (unsigned) gettid(), workload->level);

		{	// scope
			EngineContextHolder tdbb(&workload->statusWrapper, workload->attachment.getPtr(), FB_FUNCTION);

			if (workload->lock->lck_logical != LCK_none)
				LCK_release(tdbb, workload->lock);
		}

		workload->attachment->detach(&workload->statusWrapper);
		workload->localStatus.check();

		delete workload;
	}

	initAttachment->dropDatabase(&initStatusWrapper);
	initLocalStatus.check();

	initAttachment = nullptr;

	provider->shutdown(&initStatusWrapper, 0, fb_shutrsn_app_stopped);
	initLocalStatus.check();
}

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

	AutoPlugin<JProvider> provider(JProvider::getInstance());

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

	BOOST_TEST(counter == THREAD_COUNT * ITER_COUNT);

	initAttachment->dropDatabase(&initStatusWrapper);
	initLocalStatus.check();

	initAttachment = nullptr;

	provider->shutdown(&initStatusWrapper, 0, fb_shutrsn_app_stopped);
	initLocalStatus.check();
}

BOOST_AUTO_TEST_SUITE_END()	// LockTests


BOOST_AUTO_TEST_SUITE_END()	// LockSuite
BOOST_AUTO_TEST_SUITE_END()	// EngineSuite
