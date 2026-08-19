#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>

#include <igsc_lib.h>

namespace {

constexpr uint16_t kIntelVendorId = 0x8086;
constexpr uint16_t kTestedB70DeviceId = 0xE223;
constexpr uint32_t kGfspGetConfiguration = 16;
constexpr uint32_t kGfspSetConfiguration = 15;
constexpr size_t kGfspConfigResponseMinBytes = 20;
constexpr size_t kGfspSetResponseBytes = 4;
constexpr size_t kAvailableOffset = 0;
constexpr size_t kCurrentOffset = 4;
constexpr size_t kConfigurableOffset = 8;
constexpr size_t kPendingOffset = 12;
constexpr size_t kDefaultOffset = 16;
constexpr uint8_t kPcieDowngradeBit = 1;

struct Device {
    igsc_device_info info{};
};

struct GfspConfig {
    std::array<uint8_t, 256> bytes{};
    size_t actual = 0;
    bool available = false;
    bool current = false;
    bool configurable = false;
    bool pending = false;
    bool defaultValue = false;
};

std::string hex4(uint32_t value)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    return out.str();
}

std::string pciBdf(const igsc_device_info& info)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0')
        << std::setw(4) << static_cast<unsigned>(info.domain) << ":"
        << std::setw(2) << static_cast<unsigned>(info.bus) << ":"
        << std::setw(2) << static_cast<unsigned>(info.dev) << "."
        << static_cast<unsigned>(info.func);
    return out.str();
}

const char* yesNo(bool value)
{
    return value ? "Yes" : "No";
}

const char* enabledDisabled(bool value)
{
    return value ? "ENABLED" : "Disabled";
}

bool pcieDowngradeEnabled(const uint8_t* field)
{
    return ((field[0] >> kPcieDowngradeBit) & 1u) != 0;
}

bool isSupportedB70(const igsc_device_info& info)
{
    return info.vendor_id == kIntelVendorId && info.device_id == kTestedB70DeviceId;
}

bool isRunningAsAdministrator()
{
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;

    if (AllocateAndInitializeSid(&ntAuthority,
                                 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isMember);
        FreeSid(adminGroup);
    }

    return isMember == TRUE;
}

std::vector<Device> enumerateDevices()
{
    std::vector<Device> devices;
    igsc_device_iterator* iter = nullptr;

    const int createRc = igsc_device_iterator_create(&iter);
    if (createRc != IGSC_SUCCESS) {
        std::cerr << "Failed to enumerate IGSC devices, rc=" << createRc << "\n";
        if (createRc == IGSC_ERROR_PERMISSION_DENIED) {
            std::cerr << "Try running from an Administrator terminal.\n";
        }
        return devices;
    }

    while (true) {
        Device device{};
        const int rc = igsc_device_iterator_next(iter, &device.info);
        if (rc == IGSC_SUCCESS) {
            devices.push_back(device);
            continue;
        }
        if (rc == IGSC_ERROR_DEVICE_NOT_FOUND) {
            break;
        }
        std::cerr << "Stopped enumeration after IGSC error rc=" << rc << "\n";
        break;
    }

    igsc_device_iterator_destroy(iter);
    return devices;
}

void printDeviceLine(size_t index, const Device& device)
{
    const auto& info = device.info;
    std::cout << "[" << index << "] "
              << pciBdf(info) << " "
              << hex4(info.vendor_id) << ":" << hex4(info.device_id)
              << " subsystem " << hex4(info.subsys_vendor_id) << ":" << hex4(info.subsys_device_id);

    if (isSupportedB70(info)) {
        std::cout << " Intel Arc Pro B70 (tested write target)";
    }

    std::cout << "\n";
}

int commandList()
{
    const auto devices = enumerateDevices();
    if (devices.empty()) {
        std::cout << "No IGSC GSC child devices found.\n";
        return 1;
    }

    for (size_t i = 0; i < devices.size(); ++i) {
        printDeviceLine(i, devices[i]);
    }

    return 0;
}

bool openSupportedDevice(const Device& device, igsc_device_handle& handle)
{
    const auto& info = device.info;
    if (!isSupportedB70(info)) {
        std::cerr << "Refusing operation for untested PCI ID "
                  << hex4(info.vendor_id) << ":" << hex4(info.device_id) << ".\n";
        return false;
    }

    const int rc = igsc_device_init_by_device_info(&handle, &info);
    if (rc != IGSC_SUCCESS) {
        std::cerr << "Failed to open IGSC device " << pciBdf(info) << ", rc=" << rc << "\n";
        if (rc == IGSC_ERROR_PERMISSION_DENIED) {
            std::cerr << "Run from an Administrator terminal.\n";
        }
        return false;
    }

    return true;
}

bool readGfspConfig(igsc_device_handle& handle, GfspConfig& config)
{
    uint8_t dummy = 0;
    size_t actual = 0;
    const int rc = igsc_gfsp_heci_cmd(&handle,
                                      kGfspGetConfiguration,
                                      &dummy,
                                      0,
                                      config.bytes.data(),
                                      config.bytes.size(),
                                      &actual);

    if (rc != IGSC_SUCCESS) {
        std::cerr << "GFSP command 16 read failed, rc=" << rc << "\n";
        if (rc == IGSC_ERROR_PERMISSION_DENIED) {
            std::cerr << "Run from an Administrator terminal.\n";
        }
        return false;
    }

    if (actual < kGfspConfigResponseMinBytes) {
        std::cerr << "GFSP command 16 returned " << actual
                  << " bytes; expected at least " << kGfspConfigResponseMinBytes << ".\n";
        return false;
    }

    config.actual = actual;
    config.available = pcieDowngradeEnabled(config.bytes.data() + kAvailableOffset);
    config.current = pcieDowngradeEnabled(config.bytes.data() + kCurrentOffset);
    config.configurable = pcieDowngradeEnabled(config.bytes.data() + kConfigurableOffset);
    config.pending = pcieDowngradeEnabled(config.bytes.data() + kPendingOffset);
    config.defaultValue = pcieDowngradeEnabled(config.bytes.data() + kDefaultOffset);

    return true;
}

void printStatusDetails(const igsc_device_info& info, const GfspConfig& config)
{
    std::cout << "Intel Arc Pro B70\n";
    std::cout << "PCI: " << pciBdf(info) << "\n";
    std::cout << "Device ID: " << hex4(info.vendor_id) << ":" << hex4(info.device_id) << "\n\n";
    std::cout << "PCIe Gen4 Downgrade\n";
    std::cout << "-------------------\n";
    std::cout << "Available:     " << yesNo(config.available) << "\n";
    std::cout << "Configurable:  " << yesNo(config.configurable) << "\n";
    std::cout << "Current:       " << enabledDisabled(config.current) << "\n";
    std::cout << "Pending:       " << enabledDisabled(config.pending) << "\n";
    std::cout << "Default:       " << enabledDisabled(config.defaultValue) << "\n\n";
}

int printStatus(const Device& device)
{
    igsc_device_handle handle{};
    if (!openSupportedDevice(device, handle)) {
        return 1;
    }

    GfspConfig config{};
    const bool readOk = readGfspConfig(handle, config);
    igsc_device_close(&handle);
    if (!readOk) {
        return 1;
    }

    printStatusDetails(device.info, config);

    if (config.current || config.pending) {
        std::cout << "WARNING: This firmware setting can force the GPU to PCIe Gen4.\n";
        std::cout << "Use \"B70Pcie disable\" to request the default state.\n";
    } else {
        std::cout << "No change required.\n";
    }

    return 0;
}

int commandStatus()
{
    const auto devices = enumerateDevices();
    if (devices.empty()) {
        std::cout << "No IGSC GSC child devices found.\n";
        return 1;
    }

    for (const auto& device : devices) {
        if (device.info.vendor_id == kIntelVendorId && device.info.device_id == kTestedB70DeviceId) {
            return printStatus(device);
        }
    }

    std::cout << "No supported Intel Arc Pro B70 IGSC device found.\n";
    std::cout << "Supported read-only PCI ID: 8086:E223\n";
    return 1;
}

int commandWrite(bool requestedEnabled)
{
    std::cout << "IMPORTANT: Save all work and close running applications before continuing.\n";
    std::cout << "This operation changes a persistent GPU firmware configuration value and may require a cold shutdown.\n\n";

    if (!isRunningAsAdministrator()) {
        std::cerr << "Writes require an Administrator terminal.\n";
        return 1;
    }

    const auto devices = enumerateDevices();
    if (devices.empty()) {
        std::cout << "No IGSC GSC child devices found.\n";
        return 1;
    }

    std::vector<size_t> supportedIndexes;
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];
        if (isSupportedB70(device.info)) {
            supportedIndexes.push_back(i);
        }
    }

    if (supportedIndexes.empty()) {
        std::cout << "No supported Intel Arc Pro B70 IGSC device found.\n";
        std::cout << "Supported write PCI ID: 8086:E223\n";
        return 1;
    }

    if (supportedIndexes.size() > 1) {
        std::cerr << "Refusing write: multiple supported Intel Arc Pro B70 IGSC devices were found.\n";
        std::cerr << "Use \"B70Pcie.exe list\" to inspect the enumerated devices.\n";
        std::cerr << "Indexed writes are not implemented yet, so no firmware setting was changed.\n\n";
        for (const size_t index : supportedIndexes) {
            printDeviceLine(index, devices[index]);
        }
        return 1;
    }

    const Device* selected = &devices[supportedIndexes[0]];

    igsc_device_handle handle{};
    if (!openSupportedDevice(*selected, handle)) {
        return 1;
    }

    GfspConfig before{};
    if (!readGfspConfig(handle, before)) {
        igsc_device_close(&handle);
        return 1;
    }

    if (!before.available) {
        std::cerr << "Refusing write: PCIe Gen4 Downgrade bit is not Available.\n";
        igsc_device_close(&handle);
        return 1;
    }
    if (!before.configurable) {
        std::cerr << "Refusing write: PCIe Gen4 Downgrade bit is not Configurable.\n";
        igsc_device_close(&handle);
        return 1;
    }

    const bool pendingAlreadyRequested = before.pending == requestedEnabled;
    printStatusDetails(selected->info, before);
    std::cout << "Requested Pending state: " << enabledDisabled(requestedEnabled) << "\n\n";

    if (pendingAlreadyRequested) {
        std::cout << "Pending already matches the requested state. No write sent.\n";
        if (before.current != before.pending) {
            std::cout << "Cold shutdown/power-on may still be required before Current matches Pending.\n";
        }
        igsc_device_close(&handle);
        return 0;
    }

    std::cout << "Write request will copy the existing 4-byte Pending field and change only bit 1.\n";
    std::cout << "Current Pending:  " << enabledDisabled(before.pending) << "\n";
    std::cout << "Proposed Pending: " << enabledDisabled(requestedEnabled) << "\n\n";
    std::cout << "Type YES to write this pending firmware setting: ";

    std::string confirmation;
    std::getline(std::cin, confirmation);
    if (confirmation != "YES") {
        std::cout << "Confirmation was not YES. No write sent.\n";
        igsc_device_close(&handle);
        return 1;
    }

    GfspConfig fresh{};
    if (!readGfspConfig(handle, fresh)) {
        igsc_device_close(&handle);
        return 1;
    }

    if (!fresh.available) {
        std::cerr << "Refusing write: PCIe Gen4 Downgrade bit is no longer Available.\n";
        igsc_device_close(&handle);
        return 1;
    }
    if (!fresh.configurable) {
        std::cerr << "Refusing write: PCIe Gen4 Downgrade bit is no longer Configurable.\n";
        igsc_device_close(&handle);
        return 1;
    }

    if (fresh.pending == requestedEnabled) {
        std::cout << "Pending now matches the requested state. No write sent.\n";
        if (fresh.current != fresh.pending) {
            std::cout << "Cold shutdown/power-on may still be required before Current matches Pending.\n";
        }
        igsc_device_close(&handle);
        return 0;
    }

    std::array<uint8_t, 4> request{};
    std::memcpy(request.data(), fresh.bytes.data() + kPendingOffset, request.size());
    if (requestedEnabled) {
        request[0] |= static_cast<uint8_t>(1u << kPcieDowngradeBit);
    } else {
        request[0] &= static_cast<uint8_t>(~(1u << kPcieDowngradeBit));
    }

    std::array<uint8_t, 4> originalPending{};
    std::memcpy(originalPending.data(), fresh.bytes.data() + kPendingOffset, originalPending.size());

    const uint8_t allowedMask = static_cast<uint8_t>(1u << kPcieDowngradeBit);
    if (((request[0] ^ originalPending[0]) & static_cast<uint8_t>(~allowedMask)) != 0 ||
        request[1] != originalPending[1] ||
        request[2] != originalPending[2] ||
        request[3] != originalPending[3]) {
        std::cerr << "INTERNAL SAFETY CHECK FAILED: "
                     "write request changes bits other than PCIe Downgrade bit 1.\n";
        igsc_device_close(&handle);
        return 1;
    }

    std::array<uint8_t, 16> setResponse{};
    size_t actualSetResponse = 0;
    const int writeRc = igsc_gfsp_heci_cmd(&handle,
                                           kGfspSetConfiguration,
                                           request.data(),
                                           request.size(),
                                           setResponse.data(),
                                           setResponse.size(),
                                           &actualSetResponse);
    if (writeRc != IGSC_SUCCESS) {
        std::cerr << "GFSP command 15 write failed, rc=" << writeRc << "\n";
        igsc_device_close(&handle);
        return 1;
    }

    if (actualSetResponse != kGfspSetResponseBytes) {
        std::cerr << "GFSP command 15 returned " << actualSetResponse
                  << " bytes; expected exactly " << kGfspSetResponseBytes << ".\n";
        igsc_device_close(&handle);
        return 1;
    }

    GfspConfig after{};
    if (!readGfspConfig(handle, after)) {
        igsc_device_close(&handle);
        return 1;
    }

    igsc_device_close(&handle);

    std::array<uint8_t, 4> afterPending{};
    std::memcpy(afterPending.data(), after.bytes.data() + kPendingOffset, afterPending.size());

    if (afterPending != request) {
        std::cerr << "Write verification failed: complete Pending field "
                     "does not match the requested value.\n";
        printStatusDetails(selected->info, after);
        return 1;
    }

    std::cout << "\nWrite accepted and verified.\n\n";
    printStatusDetails(selected->info, after);
    if (after.current != requestedEnabled) {
        std::cout << "Cold shutdown/power-on is required before Current reflects the requested state.\n";
    } else {
        std::cout << "Current already reflects the requested state.\n";
    }

    return 0;
}

void printUsage()
{
    std::cout << "Usage:\n";
    std::cout << "  B70Pcie.exe list\n";
    std::cout << "  B70Pcie.exe status\n";
    std::cout << "  B70Pcie.exe disable\n";
    std::cout << "  B70Pcie.exe enable\n";
    std::cout << "\n";
    std::cout << "disable clears PCIe Gen4 Downgrade bit 1.\n";
    std::cout << "enable sets PCIe Gen4 Downgrade bit 1.\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        printUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command == "list") {
        return commandList();
    }
    if (command == "status") {
        return commandStatus();
    }
    if (command == "disable") {
        return commandWrite(false);
    }
    if (command == "enable") {
        return commandWrite(true);
    }

    printUsage();
    return 1;
}
