/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/executors/GlobalExecutor.h>

#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/portability/GTest.h>
#include <folly/synchronization/Baton.h>
#include <folly/system/HardwareConcurrency.h>

using namespace folly;

DECLARE_uint32(folly_global_io_executor_threads);
DECLARE_uint32(folly_global_cpu_executor_threads);

TEST(GlobalExecutorTest, GlobalImmutableCPUExecutor) {
  folly::Baton<> b;

  auto count = 0;
  auto f = [&]() {
    count++;
    b.post();
  };

  // Don't explode, we should create the default global CPUExecutor lazily here.
  getGlobalCPUExecutor()->add(f);
  b.wait();
  EXPECT_EQ(1, count);
}

TEST(GlobalExecutorTest, GlobalImmutableIOExecutor) {
  folly::Baton<> b;

  auto count = 0;
  auto f = [&]() {
    count++;
    b.post();
  };

  // Don't explode, we should create the default global CPUExecutor lazily here.
  getGlobalIOExecutor()->add(f);
  b.wait();
  EXPECT_EQ(1, count);
}

TEST(GlobalExecutorTest, GlobalCPUExecutor) {
  class DummyExecutor : public folly::Executor {
   public:
    void add(Func fn) override {
      fn();
      count++;
    }
    int count{0};
  };

  // The default CPU executor is a thread pool
  auto f = [&]() {};

  // Don't explode, we should create the default global CPUExecutor lazily here.
  FOLLY_PUSH_WARNING
  FOLLY_GNU_DISABLE_WARNING("-Wdeprecated-declarations")
  getCPUExecutor()->add(f);

  {
    auto dummy = std::make_shared<DummyExecutor>();
    setCPUExecutor(dummy);
    getCPUExecutor()->add(f);
    // Make sure we were properly installed.
    EXPECT_EQ(1, dummy->count);
  }

  // Don't explode, we should restore the default global CPUExecutor because our
  // weak reference to dummy has expired
  getCPUExecutor()->add(f);
  FOLLY_POP_WARNING
}

TEST(GlobalExecutorTest, GlobalIOExecutor) {
  class DummyExecutor : public IOExecutor {
   public:
    void add(Func) override { count++; }
    folly::EventBase* getEventBase() override { return nullptr; }
    int count{0};
  };

  auto f = []() {};

  FOLLY_PUSH_WARNING
  FOLLY_GNU_DISABLE_WARNING("-Wdeprecated-declarations")
  // Don't explode, we should create the default global IOExecutor lazily here.
  getIOExecutor()->add(f);

  {
    auto dummy = std::make_shared<DummyExecutor>();
    setIOExecutor(dummy);
    getIOExecutor()->add(f);
    // Make sure we were properly installed.
    EXPECT_EQ(1, dummy->count);
  }

  // Don't explode, we should restore the default global IOExecutor because our
  // weak reference to dummy has expired
  getIOExecutor()->add(f);
  FOLLY_POP_WARNING
}

TEST(GlobalExecutorTest, IOThreadCountFlagUnset) {
  gflags::FlagSaver flagsaver;

  auto io_threadpool = dynamic_cast<folly::IOThreadPoolExecutor*>(
      folly::getGlobalIOExecutor().get());

  EXPECT_EQ(io_threadpool->numThreads(), folly::hardware_concurrency());
}

TEST(GlobalExecutorTest, CPUThreadCountFlagUnset) {
  gflags::FlagSaver flagsaver;

  auto cpu_threadpool = dynamic_cast<folly::CPUThreadPoolExecutor*>(
      folly::getGlobalCPUExecutor().get());

  EXPECT_EQ(cpu_threadpool->numThreads(), folly::hardware_concurrency());
}
