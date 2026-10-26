#include "doctest.h"

#include "auid.h"

TEST_CASE("highest ID should be pinned") {
  CHECK_EQ(AuId::FirstUnreserved(), AuId::InsurerId() + 1);
}
