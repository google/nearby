// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/implementation/windows/wifi_intel.h"

// clang-format off
#include <windows.h>  // NOLINT
#include <cfgmgr32.h>
#include <devpkey.h>
#include <devpropdef.h>
#include <initguid.h> // NOLINT
#include <libloaderapi.h> // NOLINT
#include <stdbool.h>  // NOLINT
#include <wlanapi.h>  // NOLINT
#include <winreg.h>
// clang-format on

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>

#ifndef NO_INTEL_PIE
#include "absl/strings/str_format.h"
#include "third_party/intel/pie/include/PieApiErrors.h"
#include "third_party/intel/pie/include/PieApiTypes.h"
#include "third_party/intel/pie/include/PieDefinitions.h"
#include "third_party/intel/pie/include/PieErrorMacro.h"
#include "internal/platform/logging.h"
#endif

namespace nearby {
namespace windows {
namespace {

#define SAFEDELETE(x)                                                        \
  {                                                                          \
    try {                                                                    \
      if (x) {                                                               \
        delete x;                                                            \
        x = nullptr;                                                         \
      }                                                                      \
    } catch (...) {                                                          \
      LOG(INFO) << absl::StrFormat("Exception while delete memory at 0x%p ", \
                                   (void*)x);                                \
    }                                                                        \
  }

#define SAFEDELETEARRAY(x)                                                   \
  {                                                                          \
    try {                                                                    \
      if (x) {                                                               \
        delete[] x;                                                          \
        x = nullptr;                                                         \
      }                                                                      \
    } catch (...) {                                                          \
      LOG(INFO) << absl::StrFormat("Exception while delete memory at 0x%p ", \
                                   (void*)x);                                \
    }                                                                        \
  }

#define SAFEFREELIBRARY(x)                                                     \
  {                                                                            \
    try {                                                                      \
      if (x) {                                                                 \
        FreeLibrary(x);                                                        \
        x = nullptr;                                                           \
      }                                                                        \
    } catch (...) {                                                            \
      LOG(INFO) << absl::StrFormat("Exception while freeing library at 0x%p ", \
                                   (void*)x);                                  \
    }                                                                          \
  }

#ifndef NO_INTEL_PIE
#define PIE_API_DLL L"\\MurocApi.dll"
#define ERROR_
const wchar_t PIE_HW_ID_[] = L"SWC\\VID_8086&PID_PIE&SID_0001\0";
const wchar_t PIE_DLL_PATH_HINT[] = L"PiePathHint";
#endif
}  // namespace
#ifndef NO_INTEL_PIE
typedef MUROC_RET(APIENTRY* WIFIGETADAPTERLIST)(  // NOLINT
    PINTEL_WIFI_HEADER pHeader, void** pAdapterList);
typedef MUROC_RET(APIENTRY* REGISTERINTELCB)(
    MurocDefs::PINTEL_CALLBACK pIntelCallback);
typedef MUROC_RET(APIENTRY* GETRADIOSTATE)(HADAPTER hAdapter, bool* bEnabled);
typedef MUROC_RET(APIENTRY* WIFIPANQUERYPREFERREDCHANNELSETTING)(
    HADAPTER hAdapter, PINTEL_WIFI_HEADER pHeader,
    void* pOutQueryPreferredChannel);
typedef MUROC_RET(APIENTRY* WIFILEGACYGOSETSCANFILTER)(
    HADAPTER hAdapter, PINTEL_WIFI_HEADER pHeader, void* pInputData);
typedef MUROC_RET(APIENTRY* WIFIPANRESETLEGACYGOSCANFILTER)(
    HADAPTER hAdapter, PINTEL_WIFI_HEADER pHeader);
typedef MUROC_RET(APIENTRY* DEREGISTERINTELCB)(
    MurocDefs::INTEL_EVENT_CALLBACK fnCallbac);
typedef MUROC_RET(APIENTRY* FREELISTMEMORY)(void* pList);

// Forward declarations of the internal private functions
wchar_t* GetEntireRegistryDeviceList();
bool IsHwIdMatching(DEVINST devInst, const wchar_t* expecedHwId);
DEVINST SearchForDeviceInstance(wchar_t* pEntireDeviceList);
void CloseRegKeyHandle(HKEY softwareKey);  // NOLINT
void OpenRegKeyHandle(DEVINST devInst, HKEY& softwareKey);
DWORD GetRegKeyWCHARValue(DEVINST deviceInstance, LPCWSTR keyName,  // NOLINT
                          wchar_t* valOut, PDWORD pValLen,          // NOLINT
                          PDWORD pDataType);                        // NOLINT
DWORD GetFullDllLoadPathFromPieRegistry(DEVINST pieDeviceInstance,
                                        PWCHAR* ppDllPathValue);  // NOLINT
HADAPTER WifiGetAdapterList(HINSTANCE murocApiDllHandle,          // NOLINT
                            PINTEL_ADAPTER_LIST_V120* ppAllAdapters);
void RegisterIntelCallback(HINSTANCE murocApiDllHandle,
                           MurocDefs::PINTEL_CALLBACK pIntelEventCbHandle);
void DeregisterIntelCallback(HINSTANCE murocApiDllHandle,
                             MurocDefs::INTEL_EVENT_CALLBACK fnCallback);
void FreeMemoryList(HINSTANCE murocApiDllHandle, void* ptr);

void WINAPI IntelEventHandler(MurocDefs::INTEL_EVENT iEvent,  // NOLINT
                              void* pContext);
// g_intel_event_cb_handle must be the address of a global and not on the stack
// because the CB comes from another thread
MurocDefs::INTEL_CALLBACK g_intel_event_cb_handle = {IntelEventHandler,
                                                     nullptr};
#endif

WifiIntel& WifiIntel::GetInstance() {
  static std::aligned_storage_t<sizeof(WifiIntel), alignof(WifiIntel)> storage;
  static WifiIntel* instance = new (&storage) WifiIntel();
  return *instance;
}

bool WifiIntel::Start() {
  LOG(INFO) << "WifiIntel::Start()";
#ifndef NO_INTEL_PIE
  muroc_api_dll_handle_ = PIEDllLoader();
  if ((muroc_api_dll_handle_ != nullptr)) {
    LOG(INFO) << "Load PIE_API_DLL completed successfully";

    wifi_adapter_handle_ =
        WifiGetAdapterList(muroc_api_dll_handle_, &p_all_adapters_);
    if (wifi_adapter_handle_ != INVALID_HADAPTER) {
      intel_wifi_valid_ = true;
      RegisterIntelCallback(muroc_api_dll_handle_, &g_intel_event_cb_handle);
    } else {
      SAFEFREELIBRARY(muroc_api_dll_handle_);
    }
  }
#else
  LOG(INFO) << "NO_INTEL_PIE found, skip";
#endif
  return intel_wifi_valid_;
}

void WifiIntel::Stop() {
  LOG(INFO) << "WifiIntel::Stop()";
#ifndef NO_INTEL_PIE
  if (intel_wifi_valid_) {
    LOG(INFO) << "Deregister Intel Callback, free Adapters Memory "
                 "List, free Muroc Api Dll handler.";
    DeregisterIntelCallback(muroc_api_dll_handle_, IntelEventHandler);
    FreeMemoryList(muroc_api_dll_handle_, p_all_adapters_);
    SAFEFREELIBRARY(muroc_api_dll_handle_);
  }
#else
  LOG(INFO) << "NO_INTEL_PIE found, skip";
#endif
}

int WifiIntel::GetGOChannel() {
#ifndef NO_INTEL_PIE
  WIFIPANQUERYPREFERREDCHANNELSETTING WifiPanQueryPreferredChannelSettingFunc =
      nullptr;
  int channel = -1;

  DWORD dwError = ERROR_SUCCESS;               // NOLINT
  MUROC_RET murocApiRetVal = IWLAN_E_FAILURE;  // NOLINT
  INTEL_WIFI_HEADER intelWifiHeader;
  MurocDefs::INTEL_GO_OPERATION_CHANNEL_SETTING intelGOChan;

  if (!intel_wifi_valid_) return channel;

  WifiPanQueryPreferredChannelSettingFunc =
      (WIFIPANQUERYPREFERREDCHANNELSETTING)GetProcAddress(  // NOLINT
          muroc_api_dll_handle_, "WifiPanQueryPreferredChannelSetting");

  if (WifiPanQueryPreferredChannelSettingFunc == nullptr) {
    dwError = GetLastError();  // NOLINT
    LOG(INFO) << "GetProcAddress WifiPanQueryPreferredChannelSetting error: "
              << dwError;
    return channel;
  }
  VLOG(1)
      << "Load WifiPanQueryPreferredChannelSetting API completed successfully";

  intelWifiHeader.dwSize =
      sizeof(MurocDefs::INTEL_GO_OPERATION_CHANNEL_SETTING);
  memset(&intelGOChan, 0, sizeof(intelGOChan));
  murocApiRetVal = WifiPanQueryPreferredChannelSettingFunc(
      wifi_adapter_handle_, &intelWifiHeader, (void*)&intelGOChan);

  if (murocApiRetVal == IWLAN_E_SUCCESS) {  // NOLINT
    LOG(INFO) << "Calling WifiPanQueryPreferredChannelSetting API succeeded";
    if (intelGOChan.goState == MurocDefs::INTEL_GO_CURRENT_CHANNEL_ACTIVE) {
      channel = intelGOChan.channel;
    } else {
      LOG(INFO) << "No active GO found, return -1";
    }
  } else {
    LOG(INFO) << "Calling WifiPanQueryPreferredChannelSetting API "
                 "failed with error: "
              << murocApiRetVal;
  }

  return channel;
#else
  LOG(INFO) << "NO_INTEL_PIE found, return -1";
  return -1;
#endif
}

bool WifiIntel::SetScanFilter(int channel) {
#ifndef NO_INTEL_PIE
  WIFILEGACYGOSETSCANFILTER WifiLegacyGoSetScanFilterFunc = nullptr;
  DWORD dwError = ERROR_SUCCESS;               // NOLINT
  MUROC_RET murocApiRetVal = IWLAN_E_FAILURE;  // NOLINT
  INTEL_WIFI_HEADER intelWifiHeader;
  MurocDefs::WIFI_LEGACY_GO_SCAN_FILTER scanFilter;

  if (channel <= 0) return false;
  if (!intel_wifi_valid_) return false;

  LOG(INFO) << "Set scan channel:" << channel;
  WifiLegacyGoSetScanFilterFunc =
      (WIFILEGACYGOSETSCANFILTER)GetProcAddress(  // NOLINT
          muroc_api_dll_handle_, "WifiLegacyGoSetScanFilter");

  if (WifiLegacyGoSetScanFilterFunc == nullptr) {
    dwError = GetLastError();  // NOLINT
    LOG(INFO) << "GetProcAddress WifiLegacyGoSetScanFilterFunc error: "
              << dwError;
    return false;
  }
  VLOG(1) << "Load WifiLegacyGoSetScanFilterFunc API completed successfully";

  intelWifiHeader.dwSize = sizeof(MurocDefs::WIFI_LEGACY_GO_SCAN_FILTER);
  memset(&scanFilter, 0, sizeof(scanFilter));
  scanFilter.channel = (UINT8)channel;
  murocApiRetVal = WifiLegacyGoSetScanFilterFunc(
      wifi_adapter_handle_, &intelWifiHeader, (void*)&scanFilter);

  if (murocApiRetVal == IWLAN_E_SUCCESS) {  // NOLINT
    LOG(INFO) << "Calling WifiLegacyGoSetScanFilter API "
                 "succeeded, set scan channel to "
              << channel;
    return true;
  }
  LOG(INFO) << "Calling WifiLegacyGoSetScanFilter API "
               "failed with error: "
            << murocApiRetVal;

  return false;
#else
  LOG(INFO) << "NO_INTEL_PIE found, return -1";
  return false;
#endif
}

bool WifiIntel::ResetScanFilter() {
#ifndef NO_INTEL_PIE
  WIFIPANRESETLEGACYGOSCANFILTER WifiPanReSetLegacyGoScanFilterFunc = nullptr;
  DWORD dwError = ERROR_SUCCESS;               // NOLINT
  MUROC_RET murocApiRetVal = IWLAN_E_FAILURE;  // NOLINT
  INTEL_WIFI_HEADER intelWifiHeader;

  if (!intel_wifi_valid_) return false;

  WifiPanReSetLegacyGoScanFilterFunc =
      (WIFIPANRESETLEGACYGOSCANFILTER)GetProcAddress(  // NOLINT
          muroc_api_dll_handle_, "WifiPanReSetLegacyGoScanFilter");

  if (WifiPanReSetLegacyGoScanFilterFunc == nullptr) {
    dwError = GetLastError();  // NOLINT
    LOG(INFO) << "GetProcAddress WifiPanReSetLegacyGoScanFilterFunc error: "
              << dwError;
    return false;
  }
  VLOG(1)
      << "Load WifiPanReSetLegacyGoScanFilterFunc API completed successfully";
  intelWifiHeader.dwSize = 0;
  murocApiRetVal = WifiPanReSetLegacyGoScanFilterFunc(wifi_adapter_handle_,
                                                      &intelWifiHeader);

  if (murocApiRetVal == IWLAN_E_SUCCESS) {  // NOLINT
    LOG(INFO) << "Calling WifiPanReSetLegacyGoScanFilter API succeeded";
    return true;
  }
  LOG(INFO) << "Calling WifiPanQueryPreferredChannelSetting API "
               "failed with error: "
            << murocApiRetVal;

  return false;
#else
  LOG(INFO) << "NO_INTEL_PIE found, return -1";
  return false;
#endif
}

#ifndef NO_INTEL_PIE
wchar_t* GetEntireRegistryDeviceList() {
  CONFIGRET configRet = CR_SUCCESS;
  wchar_t* pDeviceList = nullptr;
  ULONG deviceListLength = 0;  // NOLINT

  // retrieves the buffer size required to hold a list of device instance IDs
  // for the local machine's device instances.
  configRet = CM_Get_Device_ID_List_SizeW(&deviceListLength, nullptr,
                                          CM_GETIDLIST_FILTER_PRESENT);

  if (configRet == CR_SUCCESS) {
    // Allocates a block of memory from a heap.for the Devices List
    pDeviceList =
        (wchar_t*)new BYTE[deviceListLength * sizeof(wchar_t)];  // NOLINT
    if (nullptr != pDeviceList) {
      // retrieves a list of device instance IDs for the local computer's device
      // instances
      configRet = CM_Get_Device_ID_ListW(nullptr, pDeviceList, deviceListLength,
                                         CM_GETIDLIST_FILTER_PRESENT);
      if (configRet != CR_SUCCESS) {
        LOG(INFO) << "Unexpected error! CM_Get_Device_ID_List return Value of "
                  << configRet;
        SAFEDELETEARRAY(pDeviceList);
      }
    } else {
      configRet = CR_OUT_OF_MEMORY;
      LOG(INFO)
          << "Unexpected error! failed to allocate memory to the device list";
    }
  } else {
    LOG(INFO) << "Unexpected error! CM_Get_Device_ID_List_Size return Value of "
              << configRet;
  }

  return pDeviceList;
}

bool IsHwIdMatching(DEVINST devInst, const wchar_t* expecedHwId) {
  bool isHwIdFound = false;
  DEVPROPTYPE propertyType = DEVPROP_TYPE_STRING_LIST;
  CONFIGRET configRet;
  wchar_t currentDeviceHwId[MAX_DEVICE_ID_LEN] = {0};
  ULONG propertySize;

  // Query the Hardware ID property of the device instance
  propertySize = sizeof(currentDeviceHwId);
  configRet = CM_Get_DevNode_PropertyW(devInst, &DEVPKEY_Device_HardwareIds,
                                       &propertyType, (BYTE*)currentDeviceHwId,
                                       &propertySize, 0);
  if (configRet == CR_SUCCESS) {
    // Compare to given HW ID
    wchar_t* pdest = wcsstr(currentDeviceHwId, expecedHwId);  // NOLINT
    if (nullptr != pdest) {
      std::wcout << "wifi_intel.cc" << ":" << __LINE__
                 << "] Intel WIFI hwId is found: " << expecedHwId << std::endl;
      isHwIdFound = true;
    }
  }
  return isHwIdFound;
}

DEVINST SearchForDeviceInstance(wchar_t* pEntireDeviceList) {
  DEVINST devInst = NULL;  // NOLINT

  if (pEntireDeviceList != nullptr) {
    bool isMatchingDeviceFound = false;
    wchar_t* currentDevice = nullptr;
    CONFIGRET configRet = CR_SUCCESS;

    // Loop - over the devices List and Find PIE device by HW ID
    for (currentDevice = pEntireDeviceList;
         (0 != *currentDevice) && (!isMatchingDeviceFound);
         currentDevice += wcslen(currentDevice) + 1) {  // NOLINT
      // If the list of devices also includes non-present devices,
      // CM_LOCATE_DEVNODE_PHANTOM should be used in place of
      // CM_LOCATE_DEVNODE_NORMAL.
      configRet =
          CM_Locate_DevNodeW(&devInst, currentDevice, CM_LOCATE_DEVNODE_NORMAL);
      if (configRet != CR_SUCCESS) {
        LOG(INFO) << "Unexpected error! CM_Locate_DevNode return Value of "
                  << configRet;
        devInst = NULL;
        break;
      }
      isMatchingDeviceFound = IsHwIdMatching(devInst, PIE_HW_ID_);
      if (isMatchingDeviceFound) {
        LOG(INFO) << "Intel WIFI Device is found!";
        break;
      } else {
        devInst = NULL;
      }
    }
  }

  return devInst;
}

void CloseRegKeyHandle(HKEY softwareKey) {
  // close the registry
  RegCloseKey(softwareKey);
}

void OpenRegKeyHandle(DEVINST devInst, HKEY& softwareKey) {
  CONFIGRET configRet = CR_SUCCESS;

  if (devInst != NULL) {
    // opens a registry key for device-specific configuration information.
    configRet = CM_Open_DevNode_Key(devInst, KEY_READ, 0,  // NOLINT
                                    RegDisposition_OpenExisting, &softwareKey,
                                    CM_REGISTRY_SOFTWARE);

    VLOG(1) << absl::StrFormat("softwareKey %p ", softwareKey);

    if (configRet != CR_SUCCESS) {
      LOG(INFO) << "Unexpected error! CM_Open_DevNode_Key return Value of "
                << configRet;
    }
  } else {
    LOG(INFO) << "devInst is NULL";
  }
}

DWORD GetRegKeyWCHARValue(DEVINST deviceInstance, LPCWSTR keyName,
                          wchar_t* valOut, PDWORD pValLen, PDWORD pDataType) {
  HKEY softwareKey;
  DWORD ret = ERROR_SUCCESS;

  if (deviceInstance != NULL) {
    OpenRegKeyHandle(deviceInstance, softwareKey);

    ret = RegQueryValueExW(softwareKey, keyName, nullptr, pDataType,
                           (LPBYTE)valOut, pValLen);  // NOLINT

    CloseRegKeyHandle(softwareKey);
  } else {
    LOG(INFO) << "Couldn't find dev instacne for device  :-( ";
    ret = ERROR_NOT_FOUND;  // NOLINT
  }
  return ret;
}

DWORD GetFullDllLoadPathFromPieRegistry(DEVINST pieDeviceInstance,
                                        PWCHAR* ppDllPathValue) {
  DWORD status = ERROR_SUCCESS;
  DWORD dllPathBufferLen = 0;
  DWORD regKeyDataType = 0;
  DWORD dllFullPathLen = 0;
  PWCHAR pLoadPathString = nullptr;
  std::wstring pathString = {};

  // Get the buffer size to allocate the dll load path
  status = GetRegKeyWCHARValue(pieDeviceInstance, PIE_DLL_PATH_HINT, nullptr,
                               &dllPathBufferLen, &regKeyDataType);
  if (status != ERROR_SUCCESS) {
    LOG(INFO) << "Unexpected error! GetRegKeyWCHARValue return Value of "
              << status;
    return status;
  } else {
    VLOG(1) << "Queried key length successfully!";
  }

  dllFullPathLen = (dllPathBufferLen + sizeof(PIE_API_DLL));
  VLOG(1) << "dll Full Path Length = " << dllFullPathLen;

  pLoadPathString = new wchar_t[dllFullPathLen];
  SecureZeroMemory(pLoadPathString, dllFullPathLen);  // NOLINT

  // Get the load path
  status =
      GetRegKeyWCHARValue(pieDeviceInstance, PIE_DLL_PATH_HINT, pLoadPathString,
                          &dllPathBufferLen, &regKeyDataType);
  if (status != ERROR_SUCCESS) {
    LOG(INFO) << "Unexpected error! GetRegKeyWCHARValue return Value of "
              << status;
    SAFEDELETEARRAY(pLoadPathString);
    return status;
  } else {
    pathString = pLoadPathString;
    VLOG(1) << "Queried key successfully!";
  }

  std::wstring fullString = pathString + PIE_API_DLL;

  wcscpy_s(pLoadPathString, dllFullPathLen, fullString.c_str());  // NOLINT

  std::wcout << "wifi_intel.cc" << ":" << __LINE__
             << "] PIE Dll Path and Name = " << pLoadPathString << std::endl;

  if (ppDllPathValue != nullptr) {
    *ppDllPathValue = pLoadPathString;
  } else {
    SAFEDELETEARRAY(pLoadPathString);
  }
  return status;
}

HINSTANCE WifiIntel::PIEDllLoader() {
  wchar_t* pEntireDeviceList = nullptr;
  DEVINST pieRegDeviceInstance = 0;
  PWCHAR pDllPathValue = nullptr;
  HINSTANCE murocApiDllHandle = nullptr;
  DWORD ret = ERROR_SUCCESS;

  pEntireDeviceList = GetEntireRegistryDeviceList();
  pieRegDeviceInstance = SearchForDeviceInstance(pEntireDeviceList);
  SAFEDELETEARRAY(pEntireDeviceList);

  ret = GetFullDllLoadPathFromPieRegistry(pieRegDeviceInstance, &pDllPathValue);

  if (ret == ERROR_SUCCESS) {
    LOG(INFO) << "Found and trying to load MurocApi.dll";

    // load the library and get the handle
    murocApiDllHandle = LoadLibraryW(pDllPathValue);  // NOLINT

    VLOG(1) << absl::StrFormat("Muroc Api Dll Handle is 0x%p ",
                               murocApiDllHandle);
  } else {
    LOG(INFO) << "GetFullDllLoadPathFromPieRegistry fails eith error: " << ret;
  }

  SAFEDELETEARRAY(pDllPathValue);

  return murocApiDllHandle;
}

HADAPTER WifiGetAdapterList(HINSTANCE murocApiDllHandle,
                            PINTEL_ADAPTER_LIST_V120* ppAllAdapters) {
  HADAPTER firstAdapterOnTheList = INVALID_HADAPTER;
  WIFIGETADAPTERLIST WifiGetAdapterListFunction = nullptr;
  DWORD dwError;

  // Use Muroc APIs - First - Get Adapter List
  WifiGetAdapterListFunction = (WIFIGETADAPTERLIST)GetProcAddress(
      murocApiDllHandle, "WifiGetAdapterList");

  if (WifiGetAdapterListFunction == nullptr) {
    dwError = GetLastError();
    LOG(INFO) << "GetProcAddress for WifiGetAdapterListFunction API "
                 "fails with error: "
              << dwError;
    return INVALID_HADAPTER;
  }

  VLOG(1) << "GetProcAddress for WifiGetAdapterListFunction API "
             "completed successfully";
  INTEL_WIFI_HEADER intelHeader = {INTEL_STRUCT_VERSION_V156,  // NOLINT
                                   sizeof(MurocDefs::INTEL_ADAPTER_LIST_V120)};
  MUROC_RET murocApiRetVal = IWLAN_E_FAILURE;

  murocApiRetVal =
      WifiGetAdapterListFunction(&intelHeader, (void**)ppAllAdapters);

  if (murocApiRetVal != IWLAN_E_SUCCESS) {
    LOG(INFO) << "Calling WifiGetAdapterListFunction API fails with error:"
              << murocApiRetVal;
    return INVALID_HADAPTER;
  }

  firstAdapterOnTheList = (*ppAllAdapters)->adapter[0].hAdapter;
  LOG(INFO) << "WIFI Adapter on the list: " << firstAdapterOnTheList;

  return firstAdapterOnTheList;
}

void RegisterIntelCallback(
    HINSTANCE murocApiDllHandle,
    const MurocDefs::PINTEL_CALLBACK pIntelEventCbHandle) {
  REGISTERINTELCB registerIntelCBFunc = nullptr;
  DWORD dwError = ERROR_SUCCESS;

  registerIntelCBFunc = (REGISTERINTELCB)GetProcAddress(
      murocApiDllHandle, "RegisterIntelCallback");
  if (registerIntelCBFunc == nullptr) {
    dwError = GetLastError();
    LOG(INFO) << "GetProcAddress of RegisterIntelCallback API fails with error:"
              << dwError;
    return;
  }

  VLOG(1) << "Load RegisterIntelCallback API successfully";
  MUROC_RET murocApiRetVal = IWLAN_E_FAILURE;

  murocApiRetVal = registerIntelCBFunc(pIntelEventCbHandle);

  if (murocApiRetVal == IWLAN_E_SUCCESS) {
    LOG(INFO) << "Calling RegisterIntelCallback API succeeded.";
  } else {
    LOG(INFO) << "Calling RegisterIntelCallback API fails with error:"
              << murocApiRetVal;
  }
}

void DeregisterIntelCallback(HINSTANCE murocApiDllHandle,
                             MurocDefs::INTEL_EVENT_CALLBACK fnCallback) {
  DEREGISTERINTELCB deregisterIntelCBFunc = nullptr;
  DWORD dwError = ERROR_SUCCESS;

  deregisterIntelCBFunc = (DEREGISTERINTELCB)GetProcAddress(
      murocApiDllHandle, "DeregisterIntelCallback");

  if (deregisterIntelCBFunc == nullptr) {
    dwError = GetLastError();
    LOG(INFO)
        << "GetProcAddress of DeregisterIntelCallback API failed with error: ",
        dwError;
    return;
  }

  {
    VLOG(1) << "Load DeregisterIntelCallback API successfully";
    MUROC_RET murocApiRetVal = IWLAN_E_FAILURE;

    murocApiRetVal = deregisterIntelCBFunc(fnCallback);

    if (murocApiRetVal == IWLAN_E_SUCCESS) {
      VLOG(1) << "Calling DeregisterIntelCallback API succeeded.";
    } else {
      LOG(INFO) << "Calling DeregisterIntelCallback API fails with error:"
                << murocApiRetVal;
    }
  }
}

void WINAPI IntelEventHandler(MurocDefs::INTEL_EVENT iEvent, void* pContext) {
  LOG(INFO) << "Received Intel Event id: %d" << iEvent.eType;
}

void FreeMemoryList(HINSTANCE murocApiDllHandle, void* ptr) {
  FREELISTMEMORY freeMemoryListFunction = nullptr;
  DWORD dwError = ERROR_SUCCESS;

  freeMemoryListFunction =
      (FREELISTMEMORY)GetProcAddress(murocApiDllHandle, "FreeListMemory");

  if (freeMemoryListFunction == nullptr) {
    dwError = GetLastError();
    LOG(INFO) << "GetProcAddress of FreeListMemory API failed with error: "
              << dwError;
  }

  if ((freeMemoryListFunction != nullptr)) {
    VLOG(1) << "Load FreeListMemory API successfully";
    MUROC_RET murocApiRetVal = IWLAN_E_FAILURE;

    murocApiRetVal = freeMemoryListFunction(ptr);

    if (murocApiRetVal == IWLAN_E_SUCCESS) {
      VLOG(1) << "Calling FreeListMemory API succeeded.";
    } else {
      LOG(INFO) << "Calling FreeListMemory API failed with error: "
                << murocApiRetVal;
    }
  }
}
#endif
}  // namespace windows
}  // namespace nearby
