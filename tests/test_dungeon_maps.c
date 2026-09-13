#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define ANCHOR_DUNGEON_MAPS_HOST_TEST
#include "../src/anchor_dungeon_maps.c"

static const void *pointers[8192];
static unsigned int pointer_count;
static unsigned char system_bytes[0xd0000] __attribute__((aligned(8)));
unsigned char *D_8015C5C8_15D1C8 = system_bytes;
static unsigned short native_sheet[MAP_SHEET_PIXELS];
static unsigned char render_arena[0x4000] __attribute__((aligned(16)));
MapSceneResource D_80167FC0_168BC0[48];
static const char *roster = "[]";
static int fail_alloc, file_loaded = 1, native_allocations;
static void *objects[4096];
static int object_count;

void *map_test_decode(unsigned int value)
{
    unsigned int normalized = value & 0xbfffffffu;
    if (normalized >= 0x80600000u && normalized <= 0x80600000u + sizeof(render_arena))
        return render_arena + (normalized - 0x80600000u);
    if (value >= 0x80000000u) return (void *)(uintptr_t)value;
    assert(value <= pointer_count);
    return (void *)pointers[value];
}
unsigned int map_test_encode(const void *value)
{
    unsigned int i;
    uintptr_t address = (uintptr_t)value;
    if (!value) return 0;
    if (address >= (uintptr_t)render_arena && address <= (uintptr_t)render_arena + sizeof(render_arena))
        return 0x80600000u + (unsigned int)(address - (uintptr_t)render_arena);
    if (address >= 0x80000000u && address <= 0xffffffffu) return (unsigned int)address;
    for (i = 1; i <= pointer_count; ++i) if (pointers[i] == value) return i;
    assert(pointer_count + 1 < 8192);
    pointers[++pointer_count] = value;
    return pointer_count;
}
void *recomp_alloc(unsigned long size) { return fail_alloc ? 0 : malloc(size); }
void recomp_free(void *p) { free(p); }
char *anchor_get_lobby_positions_json(void)
{
    char *copy;
    if (!roster) return 0;
    copy = malloc(strlen(roster) + 1);
    strcpy(copy, roster);
    return copy;
}
int func_800141C4_14DC4(unsigned int file) { assert(file == 0x7f); return file_loaded ? 1 : -1; }
int func_80014840_15440(int address, unsigned int file)
{
    assert(file_loaded && file == 0x7f && address == 0xa000000);
    return map_test_encode(native_sheet);
}
void *func_80035EEC_36AEC(void *task, short kind, unsigned int count)
{
    void *record, *last = map_pointer(task, 0x1c);
    assert(count == 1 && (kind == 1 || kind == 2));
    if (fail_alloc) return 0;
    record = calloc(1, kind == 1 ? 0x3c : 0x98);
    objects[object_count++] = record;
    ++native_allocations;
    MAP_BYTE(record, 4) = kind;
    if (last) map_set_pointer(last, 0, record);
    else map_set_pointer(task, 0x18, record);
    map_set_pointer(task, 0x1c, record);
    return record;
}
void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int animation,
    float x, float y, float z, short rx, short ry, short rz,
    float sx, float sy, float sz, short file, short animation_file)
{
    assert(model >= 0x48000040u && model <= 0x480002e0u && animation == 0xa0216ae0u);
    assert(x == 0 && y == 0 && z == 0 && rx == (short)0x8000 && ry == rx && rz == rx);
    assert(sx == 3 && sy == 3 && sz == 3 && file == 0x4d1 && animation_file == 0);
    return func_80035EEC_36AEC(task, 2, 1);
}
void func_800115E0_121E0(void *object, int x, int y)
{
    MAP_I32(object, 0x80) = x;
    MAP_I32(object, 0x84) = y;
}

#define PEER(cid, room, y, ch) "{\"cid\":" #cid ",\"room\":" #room ",\"mr\":" #room ",\"mhp\":1,\"mx\":10000,\"my\":" #y ",\"mz\":20000,\"ch\":" #ch "}"

static void test_rooms_and_height_boundaries(void)
{
    const int thresholds[] = {0, -15000, -8000, 13000, 12000, 30500, 39200, -11500, -12000, 10000};
    const int low[] = {0,0,1,1,0,2,2,1,1,2}, high[] = {0,1,2,2,1,3,3,2,2,3};
    const int floor_counts[] = {3,4,4,4,2};
    unsigned int i;
    int dungeon = -1;
    for (i = 0; i < sizeof(s_dungeon_rooms)/sizeof(s_dungeon_rooms[0]); ++i)
    {
        const unsigned char *row = s_dungeon_rooms[i];
        int rule = row[3], floor = dungeon_floor(row[1], 0, &dungeon);
        assert(dungeon == row[0] && floor >= 0 && floor < floor_counts[dungeon]);
        if (rule)
        {
            assert(dungeon_floor(row[1], thresholds[rule], &dungeon) == low[rule]);
            assert(dungeon_floor(row[1], thresholds[rule] + 1, &dungeon) == high[rule]);
        }
    }
    assert(dungeon_floor(0x91, -37200, &dungeon) == 0);
    assert(dungeon_floor(0x91, -37199, &dungeon) == 1);
    assert(dungeon_floor(0x226, 0, &dungeon) == -1);
    assert(dungeon_floor(-1, 0, &dungeon) == -1);
    assert(dungeon_floor(0x10000, 0, &dungeon) == -1);
}

static void test_projection(void)
{
    int x, y;
    assert(project_minimap(10000, 20000, 1280, 0, 48, 48, 0, &x, &y) && x == 51 && y == 41);
    assert(project_minimap(10000, 20000, 1280, 0x100, 48, 48, 0, &x, &y) && x == 81 && y == 51);
    assert(project_minimap(10000, 20000, 1280, 0x200, 48, 48, 0, &x, &y) && x == 71 && y == 80);
    assert(project_minimap(10000, 20000, 1280, 0x200, 48, 48, 1, &x, &y) && x == 71 && y == 81);
    assert(project_minimap(10000, 20000, 1280, 0x300, 48, 48, 0, &x, &y) && x == 41 && y == 71);
    assert(project_minimap(199, -199, 128, 0, 48, 48, 0, &x, &y) && x == 60 && y == 62);
    assert(project_minimap(INT_MIN, INT_MAX, 1, 0, 48, 48, 1, &x, &y) && x == 144 && y == 8);
    assert(!project_minimap(0, 0, 0, 0, 0, 0, 0, &x, &y));
}

static void test_parser(void)
{
    char json[] = "[" PEER(2, 12, -15000, 3) ","
        "{\"cid\":3,\"room\":12,\"mr\":12,\"mhp\":1,\"mx\":0,\"mz\":0,\"ch\":0},"
        PEER(4, 12, 0, 4) "," PEER(0, 12, 0, 1) "," PEER(5, 65536, 0, 0) "]";
    char original[sizeof(json)];
    strcpy(original, json);
    assert(parse_map_peers(json) == 1 && s_peers[0].cid == 2 && s_peers[0].y100 == -15000);
    assert(!strcmp(original, json));
}

static void test_floor_lifecycle_and_grouping(void)
{
    unsigned char task[0xf0] __attribute__((aligned(8))) = {0};
    void *old;
    int before;
    MAP_I32(task, 0x90) = 0;
    MAP_I32(task, 0x98) = 3;
    /* Two peers on local 1F, one in a vertical room on 2F, one other dungeon. */
    roster = "[" PEER(2, 0, 0, 1) "," PEER(18, 2, 0, 2) "," PEER(3, 12, 0, 3) "," PEER(4, 48, 0, 0) "]";
    anchor_dungeon_floor_begin(0,20,16,5,0,0,0,0,0,0);
    anchor_dungeon_floor_update(task);
    assert(s_floor_markers.count == 3);
    assert(MAP_I32(s_floor_markers.slots[0].object, 0x80) == 71);
    assert(MAP_I32(s_floor_markers.slots[1].object, 0x80) == 89);
    assert(MAP_I32(s_floor_markers.slots[2].object, 0x80) == 53);
    assert(MAP_I32(s_floor_markers.slots[0].object, 0x84) == 137);
    assert(MAP_I32(s_floor_markers.slots[2].object, 0x84) == 109);
    assert((unsigned int)MAP_I32(s_floor_markers.slots[2].object,0x2c) == 0x480002e0u);
    old = s_floor_markers.slots[0].object;
    before = native_allocations;
    /* A new lower cid must not steal the surviving higher cid's record. */
    roster = "[" PEER(1,0,0,0) "," PEER(2,0,0,2) "]";
    refresh_floor_markers(task);
    assert(s_floor_markers.slots[0].cid == 2 && s_floor_markers.slots[0].object == old);
    assert(native_allocations == before);
    assert(MAP_BYTE(s_floor_markers.slots[2].object,0x64) & 1);
    roster = 0;
    refresh_floor_markers(task);
    assert(MAP_BYTE(old,0x64) & 1);
    /* Reopen at a recycled task address; the old generation is never reused. */
    memset(task,0,sizeof(task));
    MAP_I32(task,0x90)=0; MAP_I32(task,0x98)=3;
    roster = "[" PEER(2,0,0,0) "]";
    anchor_dungeon_floor_begin(0,20,16,5,0,0,0,0,0,0);
    anchor_dungeon_floor_update(task);
    assert(s_floor_markers.slots[0].object != old);
}

static void mini_update(void *task)
{
    anchor_minimap_update_begin(task,0);
    anchor_minimap_update_end();
}

static void test_dot_graphics_memory_and_scene_lifetime(void)
{
    unsigned int start, end, color, pixel;
    unsigned short (*old_pixels)[MAP_DOT_PIXELS];
    assert(dot_arena_plan(0x80600003u, &start, &end));
    assert(start == 0x80600010u && end == 0x80600810u);
    assert(dot_arena_plan(0xc0600003u, &start, &end) && start == 0x80600010u);
    assert(!dot_arena_plan(0x8106d928u, &start, &end)); /* Previous mod-BSS address. */
    assert(!dot_arena_plan(0x807ffff0u, &start, &end));
    assert(!dot_arena_plan(0, &start, &end));

    memset(render_arena, 0x5a, sizeof(render_arena));
    for (pixel = 0; pixel < 48; ++pixel) D_80167FC0_168BC0[pixel].file_id = 1;
    anchor_dungeon_maps_load_resources();
    assert(!s_dot_pixels && !prepare_dot_textures()); /* Missing sentinel. */
    D_80167FC0_168BC0[2].file_id = 0;
    D_80167FC0_168BC0[2].data = (void *)(uintptr_t)0x8106d928u;
    anchor_dungeon_maps_load_resources();
    assert(!s_dot_pixels && !prepare_dot_textures()); /* Invalid GPU address. */
    D_80167FC0_168BC0[2].data = render_arena + 3;
    anchor_dungeon_maps_load_resources();
    assert(map_cpu_address(s_dot_pixels) == 0x80600010u);
    assert(map_cpu_address(D_80167FC0_168BC0[2].data) == 0x80600810u);
    for (pixel = 0; pixel < MAP_SHEET_PIXELS; ++pixel)
        native_sheet[pixel] = (unsigned short)(pixel * 31u);
    assert(prepare_dot_textures());
    for (color = 0; color < MAP_COLORS; ++color)
    {
        unsigned int address = *(unsigned int *)(s_dot_banks[color] + 8);
        /* The native RSP strips the high byte without extended RDRAM mode.
         * Every bank must still point to the same physical pixels afterward. */
        assert(address >= 0x80600010u && address + 128 <= 0x80600810u);
        assert((address & 0x00ffffffu) == address - 0x80000000u);
        assert(map_pointer(s_dot_banks[color], 8) == s_dot_pixels[color]);
        assert(MAP_U16(s_dot_banks[color], 0) == 8 && MAP_U16(s_dot_banks[color], 2) == 8);
        assert(MAP_U16(s_dot_banks[color], 12) == 0);
        for (pixel = 0; pixel < MAP_DOT_PIXELS; ++pixel)
            assert(s_dot_pixels[color][pixel] == recolor_dot(native_sheet[
                (24 + pixel / 8) * 64 + 24 + pixel % 8], color));
    }
    assert(render_arena[15] == 0x5a && render_arena[0x810] == 0x5a);
    assert(s_dot_descriptor[4] == 0 && s_dot_descriptor[5] == 0);
    old_pixels = s_dot_pixels;
    assert(prepare_dot_textures() && s_dot_pixels == old_pixels);
    assert(map_cpu_address(D_80167FC0_168BC0[2].data) == 0x80600810u);
    /* A new scene can place its reservation elsewhere; no old texture or
     * display-record pointer may survive that generation boundary. */
    D_80167FC0_168BC0[2].data = render_arena + 0x1000;
    anchor_dungeon_maps_load_resources();
    assert(!s_dot_ready && s_dot_pixels != old_pixels && s_mini_markers.count == 0);
    assert(map_cpu_address(s_dot_pixels) == 0x80601000u);
}

static void test_minimap_lifecycle_colors_and_list(void)
{
    unsigned char task[0xf0] __attribute__((aligned(8))) = {0};
    void *local = func_80035EEC_36AEC(task,1,1);
    void *background = func_80035EEC_36AEC(task,1,1);
    void *first, *second;
    int i, before;
    map_set_pointer(task,0xe0,local);
    MAP_I32(task,0xdc)=2;
    MAP_U16(system_bytes,0x3adf2)=12;
    MAP_I32(system_bytes,0x3b008)=1280;
    MAP_I32(system_bytes,0x3b000)=48; MAP_I32(system_bytes,0x3b004)=48;
    MAP_BYTE(background,0x10)=160;
    roster = "[" PEER(2,12,0,0) "," PEER(18,12,0,1) "," PEER(3,13,0,2) "]";
    for (i=0;i<MAP_SHEET_PIXELS;++i) native_sheet[i]=0xf801;
    file_loaded=0;
    mini_update(task);
    assert(!s_dot_ready && s_mini_markers.count==0);
    file_loaded=1;
    mini_update(task);
    assert(s_dot_ready && s_mini_markers.count==2);
    first=s_mini_markers.slots[0].object; second=s_mini_markers.slots[1].object;
    assert(s_mini_markers.slots[0].color != s_mini_markers.slots[1].color);
    assert(map_pointer(local,0)==background && map_pointer(background,0)==first);
    assert(map_pointer(first,0)==second && !map_pointer(second,0));
    assert(MAP_U16(first,0x1c)==51 && MAP_U16(first,0x20)==41);
    assert(MAP_BYTE(first,0x10)==160 && MAP_BYTE(local,0x10)==0);
    assert(map_pointer(first,0x14)!=map_pointer(second,0x14));
    assert(((unsigned int)MAP_I32(first,8) >> 15 & 15) == 3);
    assert(((unsigned int)MAP_I32(first,8) >> 11 & 15) == 3);
    assert(s_dot_pixels[0][0]==0x07ff && recolor_dot(0xf800,0)==0x07fe);
    assert(recolor_dot(1,0)==1 && recolor_dot(0,0)==0);
    before=native_allocations;
    MAP_I32(task,0xdc)=3; MAP_BYTE(background,0x10)=32;
    mini_update(task); assert(MAP_BYTE(first,0x10)==32);
    MAP_I32(task,0xdc)=1; mini_update(task); assert(MAP_BYTE(first,0x10)==0);
    roster="[]"; MAP_I32(task,0xdc)=2;
    mini_update(task); assert(MAP_BYTE(first,0x10)==0 && MAP_BYTE(second,0x10)==0);
    roster="[" PEER(2,12,0,0) "]";
    for(i=0;i<6;++i)mini_update(task);
    assert(native_allocations==before && MAP_BYTE(first,0x10)==32);
    assert(map_pointer(local,0)==background && map_pointer(first,0)==second);
    /* A new map generation resets stored pointers before native free/rebuild. */
    MAP_I32(task,0xdc)=0; mini_update(task);
    assert(s_mini_markers.count==0);
    memset(task,0,sizeof(task));
    local=func_80035EEC_36AEC(task,1,1); background=func_80035EEC_36AEC(task,1,1);
    map_set_pointer(task,0xe0,local); MAP_I32(task,0xdc)=2; MAP_BYTE(background,0x10)=160;
    fail_alloc=1; mini_update(task); fail_alloc=0;
    for(i=0;i<6;++i)mini_update(task);
    assert(s_mini_markers.slots[0].object && s_mini_markers.slots[0].object!=first);
    assert(map_pointer(local,0)==background);
}

int main(void)
{
    int i;
    test_rooms_and_height_boundaries(); test_projection(); test_parser();
    test_floor_lifecycle_and_grouping(); test_dot_graphics_memory_and_scene_lifetime();
    test_minimap_lifecycle_colors_and_list();
    for(i=0;i<object_count;++i) free(objects[i]);
    recomp_free(s_peers); recomp_free(s_floor_markers.slots); recomp_free(s_mini_markers.slots);
    puts("Dungeon-map tests passed: native floors, projection, roster filtering, colors, ownership, reuse, and allocation failure.");
    return 0;
}
