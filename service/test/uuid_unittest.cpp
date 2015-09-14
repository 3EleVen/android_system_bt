//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
#include <algorithm>
#include <array>
#include <stdint.h>

#include <gtest/gtest.h>

#include "service/uuid.h"

using namespace bluetooth;

namespace {

const std::array<uint8_t, UUID::kNumBytes128> kBtSigBaseUUID = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb, }
};

}  // namespace

// Verify that an uninitialized UUID is equal
// To the BT SIG Base UUID.
TEST(UUIDTest, DefaultUUID) {
  UUID uuid;
  ASSERT_TRUE(uuid.GetFullBigEndian() == kBtSigBaseUUID);
}

// Verify that we initialize a 16-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init16Bit) {
  auto my_uuid_16 = kBtSigBaseUUID;
  my_uuid_16[2] = 0xde;
  my_uuid_16[3] = 0xad;
  UUID uuid(UUID::UUID16Bit({{ 0xde, 0xad }}));
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_16);
}

// Verify that we initialize a 16-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init16BitString) {
  auto my_uuid_16 = kBtSigBaseUUID;
  my_uuid_16[2] = 0xde;
  my_uuid_16[3] = 0xad;
  UUID uuid("dead");
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_16);
}


// Verify that we initialize a 32-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init32Bit) {
  auto my_uuid_32 = kBtSigBaseUUID;
  my_uuid_32[0] = 0xde;
  my_uuid_32[1] = 0xad;
  my_uuid_32[2] = 0xbe;
  my_uuid_32[3] = 0xef;
  UUID uuid(UUID::UUID32Bit({{ 0xde, 0xad, 0xbe, 0xef }}));
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_32);
}

// Verify correct reading of a 32-bit UUID initialized from string.
TEST(UUIDTest, Init32BitString) {
  auto my_uuid_32 = kBtSigBaseUUID;
  my_uuid_32[0] = 0xde;
  my_uuid_32[1] = 0xad;
  my_uuid_32[2] = 0xbe;
  my_uuid_32[3] = 0xef;
  UUID uuid("deadbeef");
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_32);
}

// Verify that we initialize a 128-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init128Bit) {
  auto my_uuid_128 = kBtSigBaseUUID;
  for (int i = 0; i < static_cast<int>(my_uuid_128.size()); ++i) {
    my_uuid_128[i] = i;
  }

  UUID uuid(my_uuid_128);
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_128);
}

// Verify that we initialize a 128-bit UUID in a
// way consistent with how we read it as LE.
TEST(UUIDTest, Init128BitLittleEndian) {
  auto my_uuid_128 = kBtSigBaseUUID;
  for (int i = 0; i < static_cast<int>(my_uuid_128.size()); ++i) {
    my_uuid_128[i] = i;
  }

  UUID uuid(my_uuid_128);
  std::reverse(my_uuid_128.begin(), my_uuid_128.end());
  ASSERT_TRUE(uuid.GetFullLittleEndian() == my_uuid_128);
}

// Verify that we initialize a 128-bit UUID in a
// way consistent with how we read it.
TEST(UUIDTest, Init128BitString) {
  auto my_uuid_128 = kBtSigBaseUUID;
  for (int i = 0; i < static_cast<int>(my_uuid_128.size()); ++i) {
    my_uuid_128[i] = i;
  }

  std::string uuid_text("000102030405060708090A0B0C0D0E0F");
  ASSERT_TRUE(uuid_text.size() == (16 * 2));
  UUID uuid(uuid_text);
  ASSERT_TRUE(uuid.GetFullBigEndian() == my_uuid_128);
}
