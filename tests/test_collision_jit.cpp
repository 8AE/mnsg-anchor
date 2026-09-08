/* Optional packaged-code regression; run with tests/run_collision_jit.sh.
 * Uses N64Recomp's actual live compiler and a synthetic actor list. It never
 * launches the game or invokes native game functions. Host C tests cannot
 * catch a MIPS switch table being mistaken for an indirect function call. */
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "recompiler/live_recompiler.h"

static std::unordered_map<uint32_t, recomp_func_t *> functions;
static uint32_t missing_function;

[[noreturn]] static void unexpected()
{
    std::cerr << "fixture attempted an unexpected runtime call\n";
    std::exit(2);
}

static void unresolved(uint8_t *, recomp_context *ctx)
{
    ctx->r2 = static_cast<gpr>(-777);
}

static recomp_func_t *lookup(int32_t address)
{
    auto found = functions.find(static_cast<uint32_t>(address));
    if (found != functions.end())
        return found->second;
    missing_function = static_cast<uint32_t>(address);
    return unresolved;
}

static std::vector<char> read_file(const char *path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error(std::string("cannot read ") + path);
    return {std::istreambuf_iterator<char>(stream), {}};
}

/* The runtime normally obtains this map from the game's overlay registry.
 * This fixture needs only a consistent index for each referenced section. */
static std::unordered_map<uint32_t, uint16_t> reference_sections(
    const std::vector<char> &symbols)
{
    auto word = [&](size_t offset) {
        if (offset > symbols.size() || symbols.size() - offset < 4)
            throw std::runtime_error("truncated mod symbols");
        uint32_t value;
        std::memcpy(&value, symbols.data() + offset, sizeof(value));
        return value;
    };
    if (symbols.size() < 52 || std::memcmp(symbols.data(), "N64RSYMS", 8) ||
        word(8) != 1)
        throw std::runtime_error("expected version 1 N64RSYMS");
    std::unordered_map<uint32_t, uint16_t> result;
    size_t offset = 52 + static_cast<size_t>(word(48));
    for (uint32_t section = 0; section < word(12); ++section)
    {
        uint32_t count = word(offset + 24);
        offset += 28 + static_cast<size_t>(word(offset + 20)) * 8;
        for (uint32_t relocation = 0; relocation < count; ++relocation)
        {
            uint32_t rom = word(offset + static_cast<size_t>(relocation) * 16 + 12);
            if (!(rom & 0x80000000u))
                result.emplace(rom, static_cast<uint16_t>(result.size()));
        }
        offset += static_cast<size_t>(count) * 16;
    }
    return result;
}

static uint32_t collector_address(const char *map_path)
{
    std::ifstream map(map_path);
    std::string line;
    while (std::getline(map, line))
    {
        std::istringstream fields(line);
        uint32_t address;
        std::string load, size, alignment, name;
        if (fields >> std::hex >> address >> load >> size >> alignment >> name &&
            name == "anchor_collision_append_enemies")
            return address;
    }
    throw std::runtime_error("collector not found in matching mod.map");
}

static int run(const char *symbols_path, const char *binary_path,
               const char *map_path)
{
    auto symbols = read_file(symbols_path);
    auto binary = read_file(binary_path);
    auto references = reference_sections(symbols);
    N64Recomp::Context context;
    auto parsed = N64Recomp::parse_mod_symbols(
        symbols, std::span(reinterpret_cast<const uint8_t *>(binary.data()),
                           binary.size()), references, context);
    if (parsed != N64Recomp::ModSymbolsError::Good)
        throw std::runtime_error("cannot parse packaged mod");
    context.rom.assign(binary.begin(), binary.end());
    context.set_all_reference_sections_relocatable();
    context.skip_validating_reference_symbols = true;
    std::vector<int32_t> reference_addresses(65536, 0x80100000);
    std::vector<int32_t> local_addresses(context.sections.size());
    N64Recomp::LiveGeneratorInputs inputs{};
    inputs.reference_section_addresses = reference_addresses.data();
    inputs.local_section_addresses = local_addresses.data();
    inputs.get_function = lookup;
    inputs.cop0_status_write = [](recomp_context *, gpr) { unexpected(); };
    inputs.cop0_status_read = [](recomp_context *) -> gpr { unexpected(); };
    inputs.switch_error = [](const char *, uint32_t, uint32_t) { unexpected(); };
    inputs.do_break = [](uint32_t) { unexpected(); };
    inputs.syscall_handler = [](uint8_t *, recomp_context *, int32_t) { unexpected(); };
    inputs.pause_self = [](uint8_t *) { unexpected(); };
    inputs.trigger_event = [](uint8_t *, recomp_context *, uint32_t) { unexpected(); };
    inputs.run_hook = [](uint8_t *, recomp_context *, size_t) { unexpected(); };
    N64Recomp::live_recompiler_init();
    N64Recomp::LiveGenerator generator(context.functions.size(), inputs);
    std::vector<std::vector<uint32_t>> static_functions;
    for (size_t index = 0; index < context.functions.size(); ++index)
    {
        std::ostringstream unused;
        if (!N64Recomp::recompile_function_live(generator, context, index,
                                               unused, static_functions, true))
            throw std::runtime_error("packaged function failed live compilation");
    }
    auto compiled = generator.finish();
    if (!compiled.good)
        throw std::runtime_error("live compilation did not finish");
    std::vector<uint8_t> memory(0x2000000);
    for (size_t index = 0; index < context.sections.size(); ++index)
    {
        const auto &section = context.sections[index];
        uint32_t start = section.ram_addr - 0x80000000u;
        if (start >= memory.size() || section.size > memory.size() - start ||
            section.rom_addr > binary.size() ||
            section.size > binary.size() - section.rom_addr)
            throw std::runtime_error("mod exceeds fixture memory");
        local_addresses[index] = section.ram_addr;
        for (size_t byte = 0; byte < section.size; ++byte)
            memory[(start + byte) ^ 3] = binary[section.rom_addr + byte];
    }
    for (size_t index = 0; index < context.functions.size(); ++index)
        functions[context.functions[index].vram] = compiled.functions[index];
    auto entry = functions.find(collector_address(map_path));
    if (entry == functions.end())
        throw std::runtime_error("collector is absent from packaged functions");
    auto put32 = [&](uint32_t address, uint32_t value) {
        std::memcpy(memory.data() + address - 0x80000000u, &value, 4);
    };
    auto put16 = [&](uint32_t address, uint16_t value) {
        std::memcpy(memory.data() + ((address - 0x80000000u) ^ 2), &value, 2);
    };
    /* Manager -> scheduler root -> linked actor. The body is disabled, so
     * classification must return before geometry, allocation or native calls. */
    put32(0x8006d328, 0x80010000);
    put16(0x8006d334, 1);
    put32(0x80010000, 0x80011000);
    put32(0x80011004, 0x80010000);
    unsigned failures = 0;
    for (unsigned id : {0xcc, 0xcb, 0xcd, 0xfa, 0x1b0, 0x323, 0x55})
    {
        put16(0x8001105c, id);
        missing_function = 0;
        recomp_context registers{};
        registers.r4 = static_cast<gpr>(static_cast<int32_t>(0x80020000));
        registers.r5 = static_cast<gpr>(static_cast<int32_t>(0x80020004));
        registers.r29 = static_cast<gpr>(static_cast<int32_t>(0x807ff000));
        entry->second(memory.data(), &registers);
        std::cout << "actor 0x" << std::hex << id << " missing function 0x"
                  << missing_function << " result " << std::dec
                  << static_cast<int32_t>(registers.r2) << '\n';
        if (missing_function || registers.r2 != 0)
            ++failures;
    }
    std::cout << failures << "/7 packaged collision cases failed\n";
    return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: test_collision_jit mod_syms.bin mod_binary.bin mod.map\n";
        return 2;
    }
    try { return run(argv[1], argv[2], argv[3]); }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
