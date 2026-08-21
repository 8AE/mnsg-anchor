#include "utils/json_utils.h"
#include "utils/room_utils.h"
#include "utils/string_utils.h"

#define CHECK(condition)       \
    do                         \
    {                          \
        if (!(condition))      \
            return __LINE__;   \
    } while (0)

static int test_strings(void)
{
    const char *cursor;
    signed int value;

    CHECK(mnsg_string_equal("room", "room"));
    CHECK(!mnsg_string_equal("room", "team"));
    CHECK(mnsg_string_starts_with("mnsg-friends", "mnsg-"));
    CHECK(mnsg_string_find("prefix-value", "value") != 0);
    CHECK(mnsg_string_is_blank(" \t\n\v\f"));
    CHECK(!mnsg_string_is_blank(" private "));
    CHECK(!mnsg_room_id_is_valid(" \t\v\f"));
    CHECK(!mnsg_room_id_is_valid(" mnsg- "));
    CHECK(mnsg_room_id_is_valid(" friends "));
    CHECK(mnsg_string_to_s32(" 43383 ", -1) == 43383);
    CHECK(mnsg_string_to_s32("43383x", -1) == -1);
    CHECK(mnsg_string_to_s32("2147483648", -1) == -1);

    cursor = "-2147483648,";
    CHECK(mnsg_parse_s32(&cursor, &value));
    CHECK(value == (-2147483647 - 1));
    CHECK(*cursor == ',');
    return 0;
}

static int test_json_reader(void)
{
    const char *packet =
        "{\"type\":\"UPDATE_TEAM_STATE\",\"clientId\":42,"
        "\"state\":{\"hp_max\":24,\"flag\":\"fl_tsurami\"}}";
    char flag[32];
    signed int hp_max;
    unsigned int client_id;
    unsigned short room_id;

    CHECK(mnsg_json_string_equals(packet, "type", "UPDATE_TEAM_STATE"));
    CHECK(!mnsg_json_string_equals(packet, "type", "SET_FLAG"));
    CHECK(mnsg_json_get_s32(packet, "hp_max", &hp_max));
    CHECK(hp_max == 24);
    CHECK(mnsg_json_get_u32(packet, "clientId", &client_id));
    CHECK(client_id == 42u);
    CHECK(mnsg_json_get_string(packet, "flag", flag, sizeof(flag)));
    CHECK(mnsg_string_equal(flag, "fl_tsurami"));
    CHECK(mnsg_json_get_u16("{\"roomId\":65535}", "roomId", &room_id));
    CHECK(room_id == 65535u);
    CHECK(!mnsg_json_get_u16("{\"roomId\":65536}", "roomId", &room_id));
    CHECK(!mnsg_json_get_string(packet, "flag", flag, 4u));
    return 0;
}

static int test_json_writer(void)
{
    MnsgJsonObjectWriter writer;
    char buffer[80];
    char too_small[8];

    mnsg_json_writer_begin(&writer, buffer, sizeof(buffer));
    CHECK(mnsg_json_writer_add_string(&writer, "flag", "fl_\"boss"));
    CHECK(mnsg_json_writer_add_s32(&writer, "roomId", 369));
    CHECK(mnsg_json_writer_finish(&writer));
    CHECK(mnsg_string_equal(
        buffer, "{\"flag\":\"fl_\\\"boss\",\"roomId\":369}"));

    mnsg_json_writer_begin(&writer, too_small, sizeof(too_small));
    CHECK(!mnsg_json_writer_add_string(&writer, "flag", "too-long"));
    CHECK(!mnsg_json_writer_finish(&writer));

    mnsg_json_writer_begin(&writer, buffer, sizeof(buffer));
    CHECK(!mnsg_json_writer_add_string(&writer, "flag", 0));
    CHECK(!mnsg_json_writer_finish(&writer));
    return 0;
}

int main(void)
{
    int result = test_strings();

    if (result)
        return result;
    result = test_json_reader();
    if (result)
        return result;
    return test_json_writer();
}
