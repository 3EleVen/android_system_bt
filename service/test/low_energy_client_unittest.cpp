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

#include <base/macros.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "service/hal/fake_bluetooth_gatt_interface.h"
#include "service/low_energy_client.h"
#include "stack/include/bt_types.h"
#include "stack/include/hcidefs.h"

using ::testing::_;
using ::testing::Return;

namespace bluetooth {
namespace {

class MockGattHandler
    : public hal::FakeBluetoothGattInterface::TestClientHandler {
 public:
  MockGattHandler() = default;
  ~MockGattHandler() override = default;

  MOCK_METHOD1(RegisterClient, bt_status_t(bt_uuid_t*));
  MOCK_METHOD1(UnregisterClient, bt_status_t(int));
  MOCK_METHOD7(MultiAdvEnable, bt_status_t(int, int, int, int, int, int, int));
  MOCK_METHOD10(
      MultiAdvSetInstDataMock,
      bt_status_t(bool, bool, bool, int, int, char*, int, char*, int, char*));
  MOCK_METHOD1(MultiAdvDisable, bt_status_t(int));

  // GMock has macros for up to 10 arguments (11 is really just too many...).
  // For now we forward this call to a 10 argument mock, omitting the
  // |client_if| argument.
  bt_status_t MultiAdvSetInstData(
      int /* client_if */,
      bool set_scan_rsp, bool include_name,
      bool incl_txpower, int appearance,
      int manufacturer_len, char* manufacturer_data,
      int service_data_len, char* service_data,
      int service_uuid_len, char* service_uuid) {
    return MultiAdvSetInstDataMock(
        set_scan_rsp, include_name, incl_txpower, appearance,
        manufacturer_len, manufacturer_data,
        service_data_len, service_data,
        service_uuid_len, service_uuid);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGattHandler);
};

class LowEnergyClientTest : public ::testing::Test {
 public:
  LowEnergyClientTest() = default;
  ~LowEnergyClientTest() override = default;

  void SetUp() override {
    mock_handler_.reset(new MockGattHandler());
    fake_hal_gatt_iface_ = new hal::FakeBluetoothGattInterface(
        std::static_pointer_cast<
            hal::FakeBluetoothGattInterface::TestClientHandler>(mock_handler_),
        nullptr);

    hal::BluetoothGattInterface::InitializeForTesting(fake_hal_gatt_iface_);
    ble_factory_.reset(new LowEnergyClientFactory());
  }

  void TearDown() override {
    ble_factory_.reset();
    hal::BluetoothGattInterface::CleanUp();
  }

 protected:
  hal::FakeBluetoothGattInterface* fake_hal_gatt_iface_;
  std::shared_ptr<MockGattHandler> mock_handler_;
  std::unique_ptr<LowEnergyClientFactory> ble_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LowEnergyClientTest);
};

// Used for tests that operate on a pre-registered client.
class LowEnergyClientPostRegisterTest : public LowEnergyClientTest {
 public:
  LowEnergyClientPostRegisterTest() = default;
  ~LowEnergyClientPostRegisterTest() override = default;

  void SetUp() override {
    LowEnergyClientTest::SetUp();
    UUID uuid = UUID::GetRandom();
    auto callback = [&](BLEStatus status, const UUID& in_uuid,
                        std::unique_ptr<BluetoothClientInstance> in_client) {
      CHECK(in_uuid == uuid);
      CHECK(in_client.get());
      CHECK(status == BLE_STATUS_SUCCESS);

      le_client_ = std::unique_ptr<LowEnergyClient>(
          static_cast<LowEnergyClient*>(in_client.release()));
    };

    EXPECT_CALL(*mock_handler_, RegisterClient(_))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));

    ble_factory_->RegisterClient(uuid, callback);

    bt_uuid_t hal_uuid = uuid.GetBlueDroid();
    fake_hal_gatt_iface_->NotifyRegisterClientCallback(0, 0, hal_uuid);
    testing::Mock::VerifyAndClearExpectations(mock_handler_.get());
  }

  void TearDown() override {
    EXPECT_CALL(*mock_handler_, MultiAdvDisable(_))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));
    EXPECT_CALL(*mock_handler_, UnregisterClient(_))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));
    le_client_.reset();
    LowEnergyClientTest::TearDown();
  }

  void StartAdvertising() {
    ASSERT_FALSE(le_client_->IsAdvertisingStarted());
    ASSERT_FALSE(le_client_->IsStartingAdvertising());
    ASSERT_FALSE(le_client_->IsStoppingAdvertising());

    EXPECT_CALL(*mock_handler_, MultiAdvEnable(_, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));
    EXPECT_CALL(*mock_handler_,
                MultiAdvSetInstDataMock(_, _, _, _, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(BT_STATUS_SUCCESS));

    AdvertiseSettings settings;
    AdvertiseData adv, scan_rsp;
    ASSERT_TRUE(le_client_->StartAdvertising(
        settings, adv, scan_rsp, LowEnergyClient::StatusCallback()));
    ASSERT_TRUE(le_client_->IsStartingAdvertising());

    fake_hal_gatt_iface_->NotifyMultiAdvEnableCallback(
        le_client_->GetClientId(), BT_STATUS_SUCCESS);
    fake_hal_gatt_iface_->NotifyMultiAdvDataCallback(
        le_client_->GetClientId(), BT_STATUS_SUCCESS);

    ASSERT_TRUE(le_client_->IsAdvertisingStarted());
    ASSERT_FALSE(le_client_->IsStartingAdvertising());
    ASSERT_FALSE(le_client_->IsStoppingAdvertising());
  }

 protected:
  std::unique_ptr<LowEnergyClient> le_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LowEnergyClientPostRegisterTest);
};

TEST_F(LowEnergyClientTest, RegisterClient) {
  EXPECT_CALL(*mock_handler_, RegisterClient(_))
      .Times(2)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillOnce(Return(BT_STATUS_SUCCESS));

  // These will be asynchronously populated with a result when the callback
  // executes.
  BLEStatus status = BLE_STATUS_SUCCESS;
  UUID cb_uuid;
  std::unique_ptr<LowEnergyClient> client;
  int callback_count = 0;

  auto callback = [&](BLEStatus in_status, const UUID& uuid,
                      std::unique_ptr<BluetoothClientInstance> in_client) {
        status = in_status;
        cb_uuid = uuid;
        client = std::unique_ptr<LowEnergyClient>(
            static_cast<LowEnergyClient*>(in_client.release()));
        callback_count++;
      };

  UUID uuid0 = UUID::GetRandom();

  // HAL returns failure.
  EXPECT_FALSE(ble_factory_->RegisterClient(uuid0, callback));
  EXPECT_EQ(0, callback_count);

  // HAL returns success.
  EXPECT_TRUE(ble_factory_->RegisterClient(uuid0, callback));
  EXPECT_EQ(0, callback_count);

  // Calling twice with the same UUID should fail with no additional call into
  // the stack.
  EXPECT_FALSE(ble_factory_->RegisterClient(uuid0, callback));

  testing::Mock::VerifyAndClearExpectations(mock_handler_.get());

  // Call with a different UUID while one is pending.
  UUID uuid1 = UUID::GetRandom();
  EXPECT_CALL(*mock_handler_, RegisterClient(_))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_TRUE(ble_factory_->RegisterClient(uuid1, callback));

  // Trigger callback with an unknown UUID. This should get ignored.
  UUID uuid2 = UUID::GetRandom();
  bt_uuid_t hal_uuid = uuid2.GetBlueDroid();
  fake_hal_gatt_iface_->NotifyRegisterClientCallback(0, 0, hal_uuid);
  EXPECT_EQ(0, callback_count);

  // |uuid0| succeeds.
  int client_if0 = 2;  // Pick something that's not 0.
  hal_uuid = uuid0.GetBlueDroid();
  fake_hal_gatt_iface_->NotifyRegisterClientCallback(
      BT_STATUS_SUCCESS, client_if0, hal_uuid);

  EXPECT_EQ(1, callback_count);
  ASSERT_TRUE(client.get() != nullptr);  // Assert to terminate in case of error
  EXPECT_EQ(BLE_STATUS_SUCCESS, status);
  EXPECT_EQ(client_if0, client->GetClientId());
  EXPECT_EQ(uuid0, client->GetAppIdentifier());
  EXPECT_EQ(uuid0, cb_uuid);

  // The client should unregister itself when deleted.
  EXPECT_CALL(*mock_handler_, MultiAdvDisable(client_if0))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_CALL(*mock_handler_, UnregisterClient(client_if0))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  client.reset();

  testing::Mock::VerifyAndClearExpectations(mock_handler_.get());

  // |uuid1| fails.
  int client_if1 = 3;
  hal_uuid = uuid1.GetBlueDroid();
  fake_hal_gatt_iface_->NotifyRegisterClientCallback(
      BT_STATUS_FAIL, client_if1, hal_uuid);

  EXPECT_EQ(2, callback_count);
  ASSERT_TRUE(client.get() == nullptr);  // Assert to terminate in case of error
  EXPECT_EQ(BLE_STATUS_FAILURE, status);
  EXPECT_EQ(uuid1, cb_uuid);
}

TEST_F(LowEnergyClientPostRegisterTest, StartAdvertisingBasic) {
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());

  // Use default advertising settings and data.
  AdvertiseSettings settings;
  AdvertiseData adv_data, scan_rsp;
  int callback_count = 0;
  BLEStatus last_status = BLE_STATUS_FAILURE;
  auto callback = [&](BLEStatus status) {
    last_status = status;
    callback_count++;
  };

  EXPECT_CALL(*mock_handler_, MultiAdvEnable(_, _, _, _, _, _, _))
      .Times(5)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));

  // Stack call returns failure.
  EXPECT_FALSE(le_client_->StartAdvertising(
      settings, adv_data, scan_rsp, callback));
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(0, callback_count);

  // Stack call returns success.
  EXPECT_TRUE(le_client_->StartAdvertising(
      settings, adv_data, scan_rsp, callback));
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_TRUE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(0, callback_count);

  // Already starting.
  EXPECT_FALSE(le_client_->StartAdvertising(
      settings, adv_data, scan_rsp, callback));

  // Notify failure.
  fake_hal_gatt_iface_->NotifyMultiAdvEnableCallback(
      le_client_->GetClientId(), BT_STATUS_FAIL);
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(BLE_STATUS_FAILURE, last_status);

  // Try again.
  EXPECT_TRUE(le_client_->StartAdvertising(
      settings, adv_data, scan_rsp, callback));
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_TRUE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(1, callback_count);

  // Success notification should trigger advertise data update.
  EXPECT_CALL(*mock_handler_,
              MultiAdvSetInstDataMock(
                  false,  // set_scan_rsp
                  false,  // include_name
                  false,  // incl_txpower
                  _, _, _, _, _, _, _))
      .Times(3)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));

  // Notify success for enable. The procedure will fail since setting data will
  // fail.
  fake_hal_gatt_iface_->NotifyMultiAdvEnableCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(BLE_STATUS_FAILURE, last_status);

  // Try again.
  EXPECT_TRUE(le_client_->StartAdvertising(
      settings, adv_data, scan_rsp, callback));
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_TRUE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(2, callback_count);

  // Notify success for enable. the advertise data call should succeed but
  // operation will remain pending.
  fake_hal_gatt_iface_->NotifyMultiAdvEnableCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_TRUE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(2, callback_count);

  // Notify failure from advertising call.
  fake_hal_gatt_iface_->NotifyMultiAdvDataCallback(
      le_client_->GetClientId(), BT_STATUS_FAIL);
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(3, callback_count);
  EXPECT_EQ(BLE_STATUS_FAILURE, last_status);

  // Try again. Make everything succeed.
  EXPECT_TRUE(le_client_->StartAdvertising(
      settings, adv_data, scan_rsp, callback));
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_TRUE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(3, callback_count);

  fake_hal_gatt_iface_->NotifyMultiAdvEnableCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  fake_hal_gatt_iface_->NotifyMultiAdvDataCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  EXPECT_TRUE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(4, callback_count);
  EXPECT_EQ(BLE_STATUS_SUCCESS, last_status);

  // Already started.
  EXPECT_FALSE(le_client_->StartAdvertising(
      settings, adv_data, scan_rsp, callback));
}

TEST_F(LowEnergyClientPostRegisterTest, StopAdvertisingBasic) {
  AdvertiseSettings settings;

  // Not enabled.
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->StopAdvertising(LowEnergyClient::StatusCallback()));

  // Start advertising for testing.
  StartAdvertising();

  int callback_count = 0;
  BLEStatus last_status = BLE_STATUS_FAILURE;
  auto callback = [&](BLEStatus status) {
    last_status = status;
    callback_count++;
  };

  EXPECT_CALL(*mock_handler_, MultiAdvDisable(_))
      .Times(3)
      .WillOnce(Return(BT_STATUS_FAIL))
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));

  // Stack call returns failure.
  EXPECT_FALSE(le_client_->StopAdvertising(callback));
  EXPECT_TRUE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(0, callback_count);

  // Stack returns success.
  EXPECT_TRUE(le_client_->StopAdvertising(callback));
  EXPECT_TRUE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_TRUE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(0, callback_count);

  // Already disabling.
  EXPECT_FALSE(le_client_->StopAdvertising(callback));
  EXPECT_TRUE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_TRUE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(0, callback_count);

  // Notify failure.
  fake_hal_gatt_iface_->NotifyMultiAdvDisableCallback(
      le_client_->GetClientId(), BT_STATUS_FAIL);
  EXPECT_TRUE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(BLE_STATUS_FAILURE, last_status);

  // Try again.
  EXPECT_TRUE(le_client_->StopAdvertising(callback));
  EXPECT_TRUE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_TRUE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(1, callback_count);

  // Notify success.
  fake_hal_gatt_iface_->NotifyMultiAdvDisableCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());
  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(BLE_STATUS_SUCCESS, last_status);

  // Already stopped.
  EXPECT_FALSE(le_client_->StopAdvertising(callback));
}

TEST_F(LowEnergyClientPostRegisterTest, InvalidAdvertiseData) {
  const std::vector<uint8_t> data0{ 0x02, HCI_EIR_FLAGS_TYPE, 0x00 };
  const std::vector<uint8_t> data1{
      0x04, HCI_EIR_MANUFACTURER_SPECIFIC_TYPE, 0x01, 0x02, 0x00
  };
  AdvertiseData invalid_adv(data0);
  AdvertiseData valid_adv(data1);

  AdvertiseSettings settings;

  EXPECT_FALSE(le_client_->StartAdvertising(
      settings, valid_adv, invalid_adv, LowEnergyClient::StatusCallback()));
  EXPECT_FALSE(le_client_->StartAdvertising(
      settings, invalid_adv, valid_adv, LowEnergyClient::StatusCallback()));

  // Manufacturer data not correctly formatted according to spec. We let the
  // stack handle this case.
  const std::vector<uint8_t> data2{ 0x01, HCI_EIR_MANUFACTURER_SPECIFIC_TYPE };
  AdvertiseData invalid_mfc(data2);

  EXPECT_CALL(*mock_handler_, MultiAdvEnable(_, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(Return(BT_STATUS_SUCCESS));
  EXPECT_TRUE(le_client_->StartAdvertising(
      settings, invalid_mfc, valid_adv, LowEnergyClient::StatusCallback()));
}

TEST_F(LowEnergyClientPostRegisterTest, ScanResponse) {
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());
  EXPECT_FALSE(le_client_->IsStartingAdvertising());
  EXPECT_FALSE(le_client_->IsStoppingAdvertising());

  AdvertiseSettings settings(
      AdvertiseSettings::MODE_LOW_POWER,
      base::TimeDelta::FromMilliseconds(300),
      AdvertiseSettings::TX_POWER_LEVEL_MEDIUM,
      false /* connectable */);

  const std::vector<uint8_t> data0;
  const std::vector<uint8_t> data1{
      0x04, HCI_EIR_MANUFACTURER_SPECIFIC_TYPE, 0x01, 0x02, 0x00
  };

  int callback_count = 0;
  BLEStatus last_status = BLE_STATUS_FAILURE;
  auto callback = [&](BLEStatus status) {
    last_status = status;
    callback_count++;
  };

  AdvertiseData adv0(data0);
  adv0.set_include_tx_power_level(true);

  AdvertiseData adv1(data1);
  adv1.set_include_device_name(true);

  EXPECT_CALL(*mock_handler_,
              MultiAdvEnable(le_client_->GetClientId(), _, _,
                             kAdvertisingEventTypeScannable,
                             _, _, _))
      .Times(2)
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));
  EXPECT_CALL(
      *mock_handler_,
      MultiAdvSetInstDataMock(
          false,  // set_scan_rsp
          false,  // include_name
          true,  // incl_txpower,
          _,
          0,  // 0 bytes
          _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));
  EXPECT_CALL(
      *mock_handler_,
      MultiAdvSetInstDataMock(
          true,  // set_scan_rsp
          true,  // include_name
          false,  // incl_txpower,
          _,
          data1.size() - 2,  // Mfc. Specific data field bytes.
          _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(BT_STATUS_SUCCESS));

  // Enable success; Adv. data success; Scan rsp. fail.
  EXPECT_TRUE(le_client_->StartAdvertising(settings, adv0, adv1, callback));
  fake_hal_gatt_iface_->NotifyMultiAdvEnableCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  fake_hal_gatt_iface_->NotifyMultiAdvDataCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  fake_hal_gatt_iface_->NotifyMultiAdvDataCallback(
      le_client_->GetClientId(), BT_STATUS_FAIL);

  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(BLE_STATUS_FAILURE, last_status);
  EXPECT_FALSE(le_client_->IsAdvertisingStarted());

  // Second time everything succeeds.
  EXPECT_TRUE(le_client_->StartAdvertising(settings, adv0, adv1, callback));
  fake_hal_gatt_iface_->NotifyMultiAdvEnableCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  fake_hal_gatt_iface_->NotifyMultiAdvDataCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);
  fake_hal_gatt_iface_->NotifyMultiAdvDataCallback(
      le_client_->GetClientId(), BT_STATUS_SUCCESS);

  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(BLE_STATUS_SUCCESS, last_status);
  EXPECT_TRUE(le_client_->IsAdvertisingStarted());
}

}  // namespace
}  // namespace bluetooth
