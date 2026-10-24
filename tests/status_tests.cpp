// Run the real CLI against a fake IGSC transport and administrator query.
// No hardware is accessed; all firmware writes below update in-memory data only.
#include <windows.h>
BOOL WINAPI fakeCheckTokenMembership(HANDLE, PSID, PBOOL);
#define CheckTokenMembership fakeCheckTokenMembership
#define main B70PcieApplicationMain
#include "../src/main.cpp"
#undef main
#undef CheckTokenMembership

#include <initializer_list>
#include <stdexcept>

struct igsc_device_iterator {
    size_t next = 0;
};

struct igsc_lib_ctx {
    size_t index = 0;
};

namespace {

struct FakeDevice {
    igsc_device_info info{};
    int openResult = IGSC_SUCCESS;
    int readResult = IGSC_SUCCESS;
    size_t responseSize = 20;
    bool downgrade = false;
    bool available = true;
    bool configurable = true;
    std::array<uint8_t, 4> pending{0xA5, 0x12, 0x34, 0x56};
    size_t readCount = 0;
    size_t changeOnRead = 0;
    std::array<uint8_t, 4> changedPending{};
    size_t revokeOnRead = 0;
    size_t failOnRead = 0;
    int writeResult = IGSC_SUCCESS;
    size_t writeResponseSize = 4;
    bool applyWrite = true;
    bool corruptUnrelatedBit = false;
};

struct WriteRequest {
    size_t index;
    std::array<uint8_t, 4> payload;
};

std::vector<FakeDevice> fakeDevices;
std::vector<size_t> openedDevices;
std::vector<size_t> queriedDevices;
std::vector<WriteRequest> writes;
int createResult = IGSC_SUCCESS;
int enumerationEnd = IGSC_ERROR_DEVICE_NOT_FOUND;
size_t liveHandles = 0;
bool administrator = true;

FakeDevice card(uint8_t bus, bool downgrade = false, uint16_t id = 0xE223)
{
    FakeDevice device{};
    device.info.vendor_id = 0x8086;
    device.info.device_id = id;
    device.info.bus = bus;
    device.info.subsys_vendor_id = 0x8086;
    device.info.subsys_device_id = 0x1701;
    device.downgrade = downgrade;
    if (downgrade) {
        device.pending[0] |= 2;
    }
    return device;
}

void reset(std::initializer_list<FakeDevice> devices)
{
    fakeDevices = devices;
    openedDevices.clear();
    queriedDevices.clear();
    createResult = IGSC_SUCCESS;
    enumerationEnd = IGSC_ERROR_DEVICE_NOT_FOUND;
    writes.clear();
    liveHandles = 0;
    administrator = true;
}

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct CommandResult {
    int code;
    std::string output;
    std::string errors;
};

CommandResult run(std::initializer_list<std::string> arguments = {"status"},
                  const std::string& confirmation = "", bool allowWrites = false)
{
    std::ostringstream output;
    std::ostringstream errors;
    std::istringstream input(confirmation);
    auto* oldOutput = std::cout.rdbuf(output.rdbuf());
    auto* oldErrors = std::cerr.rdbuf(errors.rdbuf());
    auto* oldInput = std::cin.rdbuf(input.rdbuf());
    const auto oldInputState = std::cin.rdstate();
    std::cin.clear();
    std::vector<std::string> args{"B70Pcie.exe"};
    args.insert(args.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);
    const size_t writesBefore = writes.size();
    const int code = B70PcieApplicationMain(static_cast<int>(args.size()), argv.data());
    std::cout.rdbuf(oldOutput);
    std::cerr.rdbuf(oldErrors);
    std::cin.rdbuf(oldInput);
    std::cin.clear(oldInputState);
    require(allowWrites || writes.size() == writesBefore, "unexpected firmware write command");
    require(liveHandles == 0, "a device handle was not closed");
    return {code, output.str(), errors.str()};
}

void contains(const std::string& text, const char* expected)
{
    require(text.find(expected) != std::string::npos, expected);
}

void testSingleCard()
{
    reset({card(4)});
    const auto result = run();
    require(result.code == 0, "single-card status failed");
    contains(result.output, "[0] 0000:04:00.0 8086:E223");
    contains(result.output, "Current:       Disabled");
    contains(result.output, "No change required.");
    require(queriedDevices == std::vector<size_t>{0}, "single card not queried once");
}

void testFourCards()
{
    reset({card(3), card(4, true), card(5), card(6, true)});
    const auto result = run();
    require(result.code == 0, "four-card status failed");
    require(queriedDevices == std::vector<size_t>({0, 1, 2, 3}), "not all four cards queried");
    for (size_t i = 0; i < fakeDevices.size(); ++i) {
        const auto start = result.output.find("PCI: " + pciBdf(fakeDevices[i].info));
        require(start != std::string::npos, "card missing from status output");
        const auto end = result.output.find("\n[", start);
        const auto section = result.output.substr(start, end - start);
        contains(section, fakeDevices[i].downgrade ? "Current:       ENABLED" : "Current:       Disabled");
        contains(section, fakeDevices[i].downgrade ? "Pending:       ENABLED" : "Pending:       Disabled");
    }
    contains(result.output, "Use \"B70Pcie.exe disable 1\"");
    contains(result.output, "Use \"B70Pcie.exe disable 3\"");
}

void testMixedDevices()
{
    reset({card(1, false, 0x1234), card(4, true), card(7, false, 0x5678)});
    const auto result = run();
    require(result.code == 0, "mixed-device status failed");
    contains(result.output, "[0] 0000:01:00.0 8086:1234");
    contains(result.output, "[1] 0000:04:00.0 8086:E223");
    contains(result.output, "[2] 0000:07:00.0 8086:5678");
    contains(result.output, "Unsupported PCI ID; not queried.");
    require(openedDevices == std::vector<size_t>{1}, "unsupported device was opened");
}

void testFailuresDoNotHideLaterCards()
{
    reset({card(3), card(4), card(5), card(6)});
    fakeDevices[0].openResult = IGSC_ERROR_PERMISSION_DENIED;
    fakeDevices[1].readResult = IGSC_ERROR_INTERNAL;
    fakeDevices[2].responseSize = 16;
    const auto result = run();
    require(result.code != 0, "device failures reported success");
    require(openedDevices == std::vector<size_t>({0, 1, 2, 3}), "stopped after a failed device");
    require(queriedDevices == std::vector<size_t>({1, 2, 3}), "later device status not queried");
    contains(result.output, "Status unavailable for this device.");
    contains(result.output, "PCI: 0000:06:00.0");
    contains(result.output, "No change required.");
    contains(result.errors, "expected at least 20");
}

void testEmptyAndUnsupported()
{
    reset({});
    auto result = run();
    require(result.code != 0, "empty enumeration reported success");
    contains(result.output, "No IGSC GSC child devices found.");
    reset({card(1, false, 0x1234)});
    result = run();
    require(result.code != 0, "no supported device reported success");
    contains(result.output, "No supported Intel Arc Pro B70 IGSC device found.");
    require(openedDevices.empty(), "unsupported device was opened");
}

void testEnumerationErrors()
{
    reset({});
    createResult = IGSC_ERROR_PERMISSION_DENIED;
    auto result = run();
    require(result.code != 0, "iterator creation failure reported success");
    contains(result.errors, "Failed to enumerate IGSC devices");
    reset({card(4)});
    enumerationEnd = IGSC_ERROR_INTERNAL;
    result = run();
    require(result.code != 0, "partial enumeration reported success");
    contains(result.output, "PCI: 0000:04:00.0");
    contains(result.errors, "Device enumeration was incomplete");
    require(result.output.find("Use \"B70Pcie.exe disable") == std::string::npos,
            "partial enumeration suggested a write");
}

void testList()
{
    reset({card(1, false, 0x1234), card(4), card(7)});
    auto result = run({"list"});
    require(result.code == 0, "list failed");
    contains(result.output, "[0] 0000:01:00.0 8086:1234");
    contains(result.output, "[2] 0000:07:00.0 8086:E223");
    require(openedDevices.empty(), "list opened a device");
    enumerationEnd = IGSC_ERROR_INTERNAL;
    result = run({"list"});
    require(result.code != 0, "partial list reported success");
}

void testIndexedStatus()
{
    reset({card(3), card(4, true), card(5, true)});
    auto result = run({"status", "1"});
    require(result.code == 0, "indexed status failed");
    require(queriedDevices == std::vector<size_t>{1}, "status queried the wrong card");
    contains(result.output, "[1] 0000:04:00.0");
    contains(result.output, "Use \"B70Pcie.exe disable 1\"");
    reset({card(1, false, 0x1234), card(4)});
    result = run({"status", "0"});
    require(result.code != 0 && openedDevices.empty(), "unsupported indexed status accessed hardware");
    contains(result.output, "Unsupported PCI ID");
}

void testIndexedDisable()
{
    reset({card(3, true), card(4, true), card(5, true)});
    const auto original = fakeDevices[1].pending;
    const auto result = run({"disable", "1"}, "YES\n", true);
    require(result.code == 0 && writes.size() == 1, "indexed disable failed");
    require(writes[0].index == 1, "indexed disable wrote the wrong card");
    require(openedDevices == std::vector<size_t>{1}, "indexed disable opened another card");
    auto expected = original;
    expected[0] &= 0xFD;
    require(writes[0].payload == expected, "indexed write changed unrelated bits");
    require((fakeDevices[0].pending[0] & 2) && (fakeDevices[2].pending[0] & 2),
            "indexed disable changed another card");
    contains(result.output, "Selected device [1] at 0000:04:00.0");
    contains(result.output, "Type YES to write this pending firmware setting for device [1] at 0000:04:00.0");
    contains(result.output, "Write accepted and verified.");

    // A mixed-device list uses the same indexes as list/status, not a B70-only index.
    reset({card(1, false, 0x1234), card(4, true), card(5, true)});
    require(run({"disable", "1"}, "YES\n", true).code == 0, "mixed-device index failed");
    require(writes.size() == 1 && writes[0].index == 1, "mixed-device index targeted the wrong card");
}

void testAutomaticAdvance()
{
    reset({card(1, false, 0x1234), card(3, true), card(4, true), card(5, true)});
    // The first B70 is still Current ENABLED, but it has already been queued off.
    fakeDevices[1].pending[0] &= 0xFD;
    auto result = run({"disable"}, "YES\n", true);
    require(result.code == 0 && writes.size() == 1 && writes[0].index == 2,
            "automatic disable did not skip the already-queued card");
    contains(result.output, "Automatically selected device [2] at 0000:04:00.0");
    result = run({"disable"}, "YES\n", true);
    require(result.code == 0 && writes.size() == 2 && writes[1].index == 3,
            "second automatic disable did not advance to the next card");
    result = run({"disable"});
    require(result.code == 0 && writes.size() == 2, "all-disabled run sent a write");
    contains(result.output, "All supported B70s already have Pending Disabled. No write sent.");
    contains(result.output, "Cold shutdown/power-on may still be required");
    require(result.output.find("Type YES") == std::string::npos, "all-disabled run prompted for a write");
    for (const auto& write : writes) {
        require(write.payload == std::array<uint8_t, 4>{0xA5, 0x12, 0x34, 0x56},
                "automatic write changed unrelated bits");
    }
}

void testNoOpAndConfirmation()
{
    reset({card(3, true), card(4, true)});
    fakeDevices[1].pending[0] &= 0xFD;
    auto result = run({"disable", "1"});
    require(result.code == 0, "indexed no-op failed");
    contains(result.output, "Pending already matches the requested state. No write sent.");
    require(result.output.find("Type YES") == std::string::npos, "indexed no-op prompted for write");
    reset({card(3, true), card(4, true)});
    result = run({"disable"}, "NO\n");
    require(result.code != 0, "declined confirmation reported success");
    require(openedDevices == std::vector<size_t>{0}, "cancellation advanced to another card");
    result = run({"disable", "1"});
    require(result.code != 0, "end of input was accepted as confirmation");
    require(writes.empty(), "unconfirmed write was sent");
}

void testInvalidSelectors()
{
    for (const auto& index : {"", "-1", "+1", " 1", "1 ", "1x", "0x1", "9999999999999999999999999999"}) {
        reset({card(3, true)});
        const auto result = run({"disable", index}, "YES\n");
        require(result.code != 0 && openedDevices.empty(), "invalid index accessed a card");
        contains(result.errors, "Invalid device index");
    }
    reset({card(3, true)});
    auto result = run({"disable", "1"}, "YES\n");
    require(result.code != 0 && openedDevices.empty(), "out-of-range index accessed a card");
    contains(result.errors, "Device index 1 was not found");
    reset({card(1, false, 0x1234), card(4, true)});
    result = run({"disable", "0"}, "YES\n");
    require(result.code != 0 && openedDevices.empty(), "unsupported selector accessed a card");
    contains(result.errors, "selected device is not a supported B70");
    require(run({"list", "0"}).code != 0, "list accepted an index");
    require(run({"disable", "0", "1"}).code != 0, "extra argument was accepted");
}

void testWritePreconditions()
{
    reset({card(3, true)});
    administrator = false;
    auto result = run({"disable"}, "YES\n");
    require(result.code != 0 && openedDevices.empty(), "non-admin command accessed a card");
    contains(result.errors, "Writes require an Administrator terminal");
    reset({card(3, true)});
    enumerationEnd = IGSC_ERROR_INTERNAL;
    result = run({"disable", "0"}, "YES\n");
    require(result.code != 0 && openedDevices.empty(), "partial enumeration allowed a write");
    contains(result.errors, "device enumeration was incomplete");
    for (int failure = 0; failure < 5; ++failure) {
        reset({card(3, true), card(4, true)});
        if (failure == 0) fakeDevices[0].openResult = IGSC_ERROR_PERMISSION_DENIED;
        if (failure == 1) fakeDevices[0].readResult = IGSC_ERROR_INTERNAL;
        if (failure == 2) fakeDevices[0].responseSize = 16;
        if (failure == 3) fakeDevices[0].available = false;
        if (failure == 4) fakeDevices[0].configurable = false;
        result = run({"disable"}, "YES\n");
        require(result.code != 0, "write precondition failure reported success");
        require(openedDevices == std::vector<size_t>{0}, "failure silently selected another card");
    }
    reset({});
    require(run({"disable"}).code != 0, "no-device write reported success");
    reset({card(1, false, 0x1234)});
    require(run({"disable"}).code != 0 && openedDevices.empty(), "unsupported automatic write attempted");
}

void testFreshRead()
{
    reset({card(4, true)});
    fakeDevices[0].changeOnRead = 2;
    fakeDevices[0].changedPending = {0xFB, 0xDE, 0xAD, 0xBE};
    auto result = run({"disable", "0"}, "YES\n", true);
    require(result.code == 0 && writes.size() == 1, "fresh-pending test failed");
    require(writes[0].payload == std::array<uint8_t, 4>{0xF9, 0xDE, 0xAD, 0xBE},
            "write did not preserve the latest Pending field");
    reset({card(4, true)});
    fakeDevices[0].changeOnRead = 2;
    fakeDevices[0].changedPending = {0xA5, 0x12, 0x34, 0x56};
    result = run({"disable", "0"}, "YES\n");
    require(result.code == 0, "fresh no-op failed");
    contains(result.output, "Pending now matches the requested state. No write sent.");
    reset({card(4, true)});
    fakeDevices[0].revokeOnRead = 2;
    result = run({"disable", "0"}, "YES\n");
    require(result.code != 0, "write proceeded after configurability was revoked");
    reset({card(4, true)});
    fakeDevices[0].failOnRead = 2;
    require(run({"disable", "0"}, "YES\n").code != 0, "failed fresh read allowed a write");
}

void testWriteVerification()
{
    for (int failure = 0; failure < 5; ++failure) {
        reset({card(3, true), card(4, true)});
        if (failure == 0) fakeDevices[0].writeResult = IGSC_ERROR_INTERNAL;
        if (failure == 1) fakeDevices[0].writeResponseSize = 3;
        if (failure == 2) fakeDevices[0].applyWrite = false;
        if (failure == 3) fakeDevices[0].corruptUnrelatedBit = true;
        if (failure == 4) fakeDevices[0].failOnRead = 3;
        const auto result = run({"disable"}, "YES\n", true);
        require(result.code != 0, "write verification failure reported success");
        require(writes.size() == 1 && writes[0].index == 0, "failed write was retried or hit another card");
        require(result.output.find("Write accepted and verified.") == std::string::npos,
                "failed write was reported as verified");
    }
}

void testEnable()
{
    reset({card(3), card(4)});
    auto result = run({"enable"}, "YES\n");
    require(result.code != 0 && openedDevices.empty(), "ambiguous enable accessed a card");
    result = run({"enable", "1"}, "YES\n", true);
    require(result.code == 0 && writes.size() == 1 && writes[0].index == 1,
            "indexed enable targeted the wrong card");
    require(writes[0].payload == std::array<uint8_t, 4>{0xA7, 0x12, 0x34, 0x56},
            "enable changed unrelated bits");
    reset({card(4)});
    result = run({"enable"}, "YES\n", true);
    require(result.code == 0 && writes.size() == 1, "single-card enable regressed");
}

} // namespace

BOOL WINAPI fakeCheckTokenMembership(HANDLE, PSID, PBOOL isMember)
{
    *isMember = administrator ? TRUE : FALSE;
    return TRUE;
}

extern "C" {

int igsc_device_iterator_create(igsc_device_iterator** iter)
{
    if (createResult != IGSC_SUCCESS) {
        return createResult;
    }
    *iter = new igsc_device_iterator{};
    return IGSC_SUCCESS;
}

int igsc_device_iterator_next(igsc_device_iterator* iter, igsc_device_info* info)
{
    if (iter->next == fakeDevices.size()) {
        return enumerationEnd;
    }
    *info = fakeDevices[iter->next++].info;
    return IGSC_SUCCESS;
}

void igsc_device_iterator_destroy(igsc_device_iterator* iter)
{
    delete iter;
}

int igsc_device_init_by_device_info(igsc_device_handle* handle, const igsc_device_info* info)
{
    for (size_t i = 0; i < fakeDevices.size(); ++i) {
        if (fakeDevices[i].info.bus == info->bus) {
            openedDevices.push_back(i);
            if (fakeDevices[i].openResult != IGSC_SUCCESS) {
                return fakeDevices[i].openResult;
            }
            handle->ctx = new igsc_lib_ctx{i};
            ++liveHandles;
            return IGSC_SUCCESS;
        }
    }
    return IGSC_ERROR_DEVICE_NOT_FOUND;
}

int igsc_device_close(igsc_device_handle* handle)
{
    delete handle->ctx;
    handle->ctx = nullptr;
    --liveHandles;
    return IGSC_SUCCESS;
}

int igsc_gfsp_heci_cmd(igsc_device_handle* handle, uint32_t command,
                       uint8_t* input, size_t inputSize, uint8_t* output,
                       size_t outputSize, size_t* actual)
{
    const size_t index = handle->ctx->index;
    auto& device = fakeDevices[index];
    if (command == 15) {
        if (!input || inputSize != 4 || outputSize < 4) {
            return IGSC_ERROR_INVALID_PARAMETER;
        }
        WriteRequest write{index, {}};
        std::memcpy(write.payload.data(), input, 4);
        writes.push_back(write);
        if (device.writeResult != IGSC_SUCCESS) {
            return device.writeResult;
        }
        if (device.applyWrite) {
            device.pending = write.payload;
            if (device.corruptUnrelatedBit) {
                device.pending[1] ^= 1;
            }
        }
        std::memcpy(output, device.pending.data(), 4);
        *actual = device.writeResponseSize;
        return IGSC_SUCCESS;
    }
    if (command != 16) {
        return IGSC_ERROR_NOT_SUPPORTED;
    }
    queriedDevices.push_back(index);
    ++device.readCount;
    if (device.readResult != IGSC_SUCCESS || device.failOnRead == device.readCount) {
        if (device.readResult == IGSC_SUCCESS) {
            return IGSC_ERROR_INTERNAL;
        }
        return device.readResult;
    }
    if (!input || inputSize != 0 || outputSize < 20) {
        return IGSC_ERROR_INVALID_PARAMETER;
    }
    if (device.changeOnRead == device.readCount) {
        device.pending = device.changedPending;
    }
    if (device.revokeOnRead == device.readCount) {
        device.configurable = false;
    }
    std::memset(output, 0, outputSize);
    output[0] = device.available ? 3 : 1;
    output[8] = device.configurable ? 3 : 1;
    output[4] = device.downgrade ? 2 : 0;
    std::memcpy(output + 12, device.pending.data(), 4);
    *actual = device.responseSize;
    return IGSC_SUCCESS;
}

} // extern "C"

int main()
{
    try {
        testSingleCard();
        testFourCards();
        testMixedDevices();
        testFailuresDoNotHideLaterCards();
        testEmptyAndUnsupported();
        testEnumerationErrors();
        testList();
        testIndexedStatus();
        testIndexedDisable();
        testAutomaticAdvance();
        testNoOpAndConfirmation();
        testInvalidSelectors();
        testWritePreconditions();
        testFreshRead();
        testWriteVerification();
        testEnable();
        std::cout << "PASS: 16 CLI test groups; simulated IGSC only, no hardware writes.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
