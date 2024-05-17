/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "basic_test.h"
#include "chimaera/network/serialize.h"
#include "chimaera/task_registry/task.h"

using chi::DomainQuery;
using chi::BinaryOutputArchive;
using chi::BinaryInputArchive;
using chi::DataTransfer;
using chi::SegmentedTransfer;
using chi::Task;
using chi::TaskFlags;

struct TestObj : public Task, TaskFlags<TF_SRL_SYM> {
  char *data_p_;
  size_t data_size_;
  int a_, b_, c_;

  TestObj(int a, int b, int c, const std::vector<char> &data)
  : a_(a), b_(b), c_(c),
    data_p_(const_cast<char *>(data.data())), data_size_(data.size()),
    Task(0)  {}

  /** Duplicate message */
  void CopyStart(const TestObj &other, bool deep) {
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar.bulk(DT_SENDER_WRITE, data_p_, data_size_);
    ar(a_, b_, c_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(a_);
  }

  bool operator==(const TestObj &other) const {
    if (a_ != other.a_ || b_ != other.b_ || c_ != other.c_) {
      return false;
    }
    if (data_size_ != other.data_size_) {
      return false;
    }
    return memcmp(data_p_, other.data_p_, data_size_) == 0;
  }
};

TEST_CASE("TestSerialize") {
  std::vector<char> data(256);

  size_t ops = 8192;
  hshm::Timer t;
  t.Resume();
  for (size_t i = 0; i < ops; ++i) {
    BinaryOutputArchive<true> out_start;
    TestObj obj(25, 30, 35, std::vector<char>(256, 10));
    out_start << obj;
    SegmentedTransfer submit_xfer = out_start.Get();

    BinaryInputArchive<true> in_start(submit_xfer);
    TestObj obj2(40, 50, 60, std::vector<char>(256, 0));
    in_start >> obj2;
    REQUIRE(obj == obj2);

    obj2.a_ = 256;

    BinaryOutputArchive<false> out_end;
    out_end << obj2;
    SegmentedTransfer complete_xfer = out_end.Get();

    BinaryInputArchive<false> in_end(complete_xfer);
    in_end >> obj;
    REQUIRE(obj == obj2);
    REQUIRE(obj.a_ == 256);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps", ops / t.GetUsec());
}
