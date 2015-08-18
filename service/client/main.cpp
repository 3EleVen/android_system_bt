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

#include <iostream>
#include <string>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include "service/adapter_state.h"
#include "service/ipc/binder/IBluetooth.h"

using namespace std;

using android::sp;

using ipc::binder::IBluetooth;

namespace {

#define COLOR_OFF         "\x1B[0m"
#define COLOR_RED         "\x1B[0;91m"
#define COLOR_GREEN       "\x1B[0;92m"
#define COLOR_YELLOW      "\x1B[0;93m"
#define COLOR_BLUE        "\x1B[0;94m"
#define COLOR_MAGENTA     "\x1B[0;95m"
#define COLOR_BOLDGRAY    "\x1B[1;30m"
#define COLOR_BOLDWHITE   "\x1B[1;37m"
#define COLOR_BOLDYELLOW  "\x1B[1;93m"

const char kCommandDisable[] = "disable";
const char kCommandEnable[] = "enable";
const char kCommandGetState[] = "get-state";
const char kCommandIsEnabled[] = "is-enabled";

#define CHECK_ARGS_COUNT(args, op, num, msg) \
  if (!(args.size() op num)) { \
    PrintError(msg); \
    return; \
  }
#define CHECK_NO_ARGS(args) \
  CHECK_ARGS_COUNT(args, ==, 0, "Expected no arguments")

void PrintError(const string& message) {
  cout << COLOR_RED << message << COLOR_OFF << endl;
}

void PrintCommandStatus(bool status) {
  cout << COLOR_BOLDWHITE "Command status: " COLOR_OFF
       << (status ? (COLOR_GREEN "success") : (COLOR_RED "failure"))
       << COLOR_OFF << endl << endl;
}

void PrintFieldAndValue(const string& field, const string& value) {
  cout << COLOR_BOLDWHITE << field << ": " << COLOR_BOLDYELLOW << value
       << COLOR_OFF << endl;
}

void PrintFieldAndBoolValue(const string& field, bool value) {
  PrintFieldAndValue(field, (value ? "true" : "false"));
}

void HandleDisable(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  PrintCommandStatus(bt_iface->Disable());
}

void HandleEnable(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  PrintCommandStatus(bt_iface->Enable());
}

void HandleGetState(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  bluetooth::AdapterState state = static_cast<bluetooth::AdapterState>(
      bt_iface->GetState());
  PrintFieldAndValue("Adapter state", bluetooth::AdapterStateToString(state));
}

void HandleIsEnabled(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  bool enabled = bt_iface->IsEnabled();
  PrintFieldAndBoolValue("Adapter enabled", enabled);
}

void HandleGetLocalAddress(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  string address = bt_iface->GetAddress();
  PrintFieldAndValue("Adapter address", address);
}

void HandleSetLocalName(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_ARGS_COUNT(args, >=, 1, "No name was given");

  std::string name;
  for (const auto& arg : args)
    name += arg + " ";

  base::TrimWhitespaceASCII(name, base::TRIM_TRAILING, &name);

  PrintCommandStatus(bt_iface->SetName(name));
}

void HandleGetLocalName(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  string name = bt_iface->GetName();
  PrintFieldAndValue("Adapter name", name);
}

void HandleAdapterInfo(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  cout << COLOR_BOLDWHITE "Adapter Properties: " COLOR_OFF << endl;

  PrintFieldAndValue("\tAddress", bt_iface->GetAddress());
  PrintFieldAndValue("\tState", bluetooth::AdapterStateToString(
      static_cast<bluetooth::AdapterState>(bt_iface->GetState())));
  PrintFieldAndValue("\tName", bt_iface->GetName());
}

void HandleHelp(IBluetooth* bt_iface, const vector<string>& args);

struct {
  string command;
  void (*func)(IBluetooth*, const vector<string>& args);
  string help;
} kCommandMap[] = {
  { "help", HandleHelp, "\t\t\tDisplay this message" },
  { "disable", HandleDisable, "\t\t\tDisable Bluetooth" },
  { "enable", HandleEnable, "\t\t\tEnable Bluetooth" },
  { "get-state", HandleGetState, "\t\tGet the current adapter state" },
  { "is-enabled", HandleIsEnabled, "\t\tReturn if Bluetooth is enabled" },
  { "get-local-address", HandleGetLocalAddress,
    "\tGet the local adapter address" },
  { "set-local-name", HandleSetLocalName, "\t\tSet the local adapter name" },
  { "get-local-name", HandleGetLocalName, "\t\tGet the local adapter name" },
  { "adapter-info", HandleAdapterInfo, "\t\tPrint adapter properties" },
  {},
};

void HandleHelp(IBluetooth* /* bt_iface */, const vector<string>& /* args */) {
  cout << endl;
  for (int i = 0; kCommandMap[i].func; i++)
    cout << "\t" << kCommandMap[i].command << kCommandMap[i].help << endl;
  cout << endl;
}

}  // namespace

int main() {
  sp<IBluetooth> bt_iface = IBluetooth::getClientInterface();
  if (!bt_iface.get()) {
    LOG(ERROR) << "Failed to obtain handle on IBluetooth";
    return EXIT_FAILURE;
  }

  cout << COLOR_BOLDWHITE << "Fluoride Command-Line Interface\n" << COLOR_OFF
       << endl
       << "Type \"help\" to see possible commands.\n"
       << endl;

  while (true) {
    string command;

    cout << COLOR_BLUE << "[FCLI] " << COLOR_OFF;
    getline(cin, command);

    vector<string> args;
    base::SplitString(command, ' ', &args);

    if (args.empty())
      continue;

    // The first argument is the command while the remaning are what we pass to
    // the handler functions.
    command = args[0];
    args.erase(args.begin());

    bool command_handled = false;
    for (int i = 0; kCommandMap[i].func && !command_handled; i++) {
      if (command == kCommandMap[i].command) {
        kCommandMap[i].func(bt_iface.get(), args);
        command_handled = true;
      }
    }

    if (!command_handled)
      cout << "Unrecognized command: " << command << endl;
  }

  return EXIT_SUCCESS;
}
