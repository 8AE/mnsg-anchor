"""Compile the production roster builder against native resolver/bridge stubs.

The tested functions are extracted verbatim from enemy_sync.c. This isolates
roster enumeration from networking/rendering without copying its algorithm.
Native pointer resolution and task scheduling are not executed by this test.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r'^(?:static )?(?:void|int|unsigned short) ' + name +
                      r'\([^;]*?\)\n\{.*?^\}', source, re.M | re.S)
    if not match:
        raise AssertionError(f'Production function missing: {name}')
    return match[0]


class ResidentRosterNativeTests(unittest.TestCase):
    def test_all_native_source_lists_and_lifecycle(self):
        source = (ROOT / 'src/enemy_sync.c').read_text()
        types = source[source.index('typedef struct\n{\n    short x;'):
                       source.index('extern EnemyStageActorMetadata')]
        functions = [
            'clear_bytes', 'clear_words', 'clear_pointers', 'bitmap_test',
            'is_regular_enemy', 'signature_mix', 'instance_index',
            'forget_actor_pointer', 'clear_room_state', 'record_roster_definition',
            'record_roster_source', 'build_room_roster',
            'enemy_sync_prepare_room_roster', 'enemy_sync_register_static_enemy',
        ]
        with tempfile.TemporaryDirectory(prefix='mnsg-resident-roster-') as directory:
            path = Path(directory)
            harness = (PREFIX + types + STUBS +
                       '\n'.join(function(source, name) for name in functions) + CASES)
            (path / 'roster.c').write_text(harness)
            build = subprocess.run([
                os.environ.get('HOST_CC', 'cc'), '-std=c99', '-O2',
                '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                str(path / 'roster.c'), '-o', str(path / 'roster')],
                capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            result = subprocess.run([str(path / 'roster')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)


PREFIX = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define ENEMY_MAX_INSTANCES 256
#define ENEMY_BITMAP_BYTES 32
#define ENEMY_MAX_PARTITION_CELLS 4096
#define ENEMY_ROOM_METADATA_COUNT 800
#define ENEMY_INVALID_ROOM 0xffff
#define ACTOR_ENTITY_ID(a) (*(unsigned short *)((char *)(a)+0x5c))
#define ACTOR_STATUS(a) (*(unsigned int *)((char *)(a)+0x68))
#define ACTOR_STATUS_REMOVE_PENDING 2
#define recomp_printf(...) ((void)0)
'''

STUBS = r'''
static EnemyStageActorMetadata *D_80231300_5EC7D0[800];
static unsigned short D_800C7AB2, s_room, s_signature, s_instance_count;
static EnemyActorInstance *D_8015CDDC, *s_source_instances[256];
static EnemyActorDefinition *D_8015CDE0;
static unsigned short s_entity_ids[256];
static void *s_live_actors[256], *s_death_actors[256];
static unsigned char s_dead_bitmap[32], s_remote_kill_pending[32];
static unsigned int s_roster_valid, s_publish_pending, s_request_pending;
static unsigned int s_broadcast_pending, s_response_client_id, s_setup_yield_pending;
static int loaded = 1, resolves, definition_resolves, world_begin_count;
static unsigned int world_count, network_begins;
static EnemyActorInstance *world_sources[256];
static unsigned short world_entities[256];
static int func_800141C4_14DC4(unsigned int file) { (void)file; return loaded ? 1 : -1; }
static void *resolve_actor_data_pointer(void *p, unsigned short f) {
    assert(loaded && f); ++resolves; return p;
}
static EnemyActorDefinition *resolve_definition(void *p, unsigned short f) {
    assert(loaded && f); ++definition_resolves; return p;
}
static void anchor_world_roster_begin(unsigned int room) {
    assert(room == D_800C7AB2); ++world_begin_count; world_count = 0;
    memset(world_sources, 0, sizeof(world_sources));
    memset(world_entities, 0, sizeof(world_entities));
}
static void anchor_world_roster_add(unsigned int i, EnemyActorInstance *s,
                                   EnemyActorDefinition *d) {
    assert(i < 256 && s && d); world_sources[i] = s; world_entities[i] = d->actor_id;
}
static void anchor_world_roster_end(unsigned int count) { world_count = count; }
static void clear_advertised_room_state(void) { }
static void begin_room_network_state(void) { ++network_begins; }
'''

CASES = r'''
int main(void) {
    EnemyActorDefinition platform = {0x1fc, 0, {0,0,0}};
    EnemyActorDefinition pink = {0xfc, 0, {0,0,0}}, green = {0xfe, 0, {0,0,0}};
    EnemyActorInstance normal[2] = {0}, grouped[3] = {0}, resident[6] = {0};
    EnemyActorPartitionConfig config = {{0},2,1,1};
    void *grid[2] = {grouped, grouped};
    EnemyStageActorMetadata metadata = {resident,0,normal,grid,&config,12,0,0};
    D_800C7AB2 = 0x30; D_80231300_5EC7D0[0x30] = &metadata;
    normal[0].definition = &pink;
    grouped[0].definition = &green; grouped[1].definition = &pink;
    resident[0].definition = resident[1].definition = &platform;
    enemy_sync_prepare_room_roster();
    assert(world_count == 5 && s_instance_count == 5 && s_roster_valid);
    assert(world_sources[0] == normal && world_sources[1] == grouped);
    assert(world_sources[2] == grouped+1 && world_sources[3] == resident);
    assert(world_sources[4] == resident+1 && world_entities[3] == 0x1fc);
    assert(s_entity_ids[0] == 0xfc && s_entity_ids[1] == 0xfe && !s_entity_ids[3]);
    unsigned short signature = s_signature;
    s_dead_bitmap[0] = 2;
    enemy_sync_prepare_room_roster();
    assert(s_signature == signature && s_dead_bitmap[0] == 2);

    /* Same-room late wave attempts must not reset a valid checkpoint. */
    int begins = world_begin_count; loaded = 0;
    enemy_sync_prepare_room_roster();
    assert(world_begin_count == begins && s_signature == signature && s_dead_bitmap[0] == 2);
    loaded = 1;

    /* A resident source also referenced by normal/partition data is one slot. */
    metadata.persistent_actor_instances = grouped;
    enemy_sync_prepare_room_roster();
    assert(world_count == 3 && s_instance_count == 3 && !s_dead_bitmap[0]);
    metadata.persistent_actor_instances = resident;

    /* The native resident list has raw definitions even with no room wave. */
    metadata.actor_instances = 0; metadata.actor_partitions = 0;
    metadata.actor_partition_configuration = 0; metadata.actor_data_file_id = 0;
    int old_resolves = resolves, old_defs = definition_resolves;
    enemy_sync_prepare_room_roster();
    assert(world_count == 2 && !s_roster_valid && world_entities[1] == 0x1fc);
    assert(resolves == old_resolves && definition_resolves == old_defs);

    D_800C7AB2 = 0x131; D_80231300_5EC7D0[0x131] = &metadata;
    for (unsigned int i = 0; i < 5; ++i) resident[i].definition = i%2 ? &pink : &green;
    enemy_sync_prepare_room_roster();
    assert(world_count == 5 && s_roster_valid && s_instance_count == 5);
    assert(s_entity_ids[0] == 0xfe && s_entity_ids[1] == 0xfc && s_entity_ids[4] == 0xfe);
    /* Registration uses the resident source descriptor, not actor+70 (which
     * the native resident constructor does not populate). Reused pools detach. */
    uint64_t task[32] = {0};
    D_8015CDDC = resident+1; D_8015CDE0 = &pink; ACTOR_ENTITY_ID(task) = 0xfc;
    enemy_sync_register_static_enemy(task, resident+1);
    assert(s_live_actors[1] == task);
    D_8015CDDC = resident+3;
    enemy_sync_register_static_enemy(task, resident+3);
    assert(!s_live_actors[1] && s_live_actors[3] == task);
    s_dead_bitmap[0] = 8;
    enemy_sync_register_static_enemy(task, resident+3);
    assert(!s_live_actors[3] && (ACTOR_STATUS(task) & 2));

    /* Script-only rooms still establish an empty world scope. */
    metadata.persistent_actor_instances = 0;
    enemy_sync_prepare_room_roster();
    assert(!world_count && !s_roster_valid && s_room == 0x131);

    /* Combined lists are capacity checked before writing slot256. */
    EnemyActorInstance many[256] = {0};
    for (unsigned int i = 0; i < 254; ++i) many[i].definition = &pink;
    metadata.actor_instances = many; metadata.actor_data_file_id = 12;
    metadata.persistent_actor_instances = resident;
    enemy_sync_prepare_room_roster();
    assert(!s_roster_valid && !world_count);
    resident[2].definition = 0;
    enemy_sync_prepare_room_roster();
    assert(s_roster_valid && world_count == 256 && s_instance_count == 256);

    /* Unterminated resident input fails closed instead of streaming a prefix. */
    for (unsigned int i = 0; i < 256; ++i) many[i].definition = &pink;
    metadata.actor_instances = 0; metadata.actor_data_file_id = 0;
    metadata.persistent_actor_instances = many;
    enemy_sync_prepare_room_roster();
    assert(!s_roster_valid && !world_count);
    return 0;
}
'''
