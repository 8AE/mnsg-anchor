/* Reuse the invitation/list harness so exact return tests exercise the same
 * production entry path. Native transition fields are a separate fixture. */
#define main invites_regression_main
#include "test_minigame_invites.c"
#undef main
#define ANCHOR_MINIGAME_INVITES_HOST_TEST
#include "../src/world/anchor_minigame_invites.c"

static void *player_link;
static void set_linked_player(void)
{
    void *pointer=&player_link;player_link=player_task;
    memcpy(player_task+4,&pointer,sizeof(pointer));
    pointer=player_object;memcpy(player_task+0x18,&pointer,sizeof(pointer));
    D_801FC604_5B8514=player_task;D_801FC60C_5B851C=player_object;
}
static void native_destination(unsigned short destination,unsigned char marker)
{
    memset(system_words,0xa5,sizeof(system_words));
    memcpy(D_8015C5C8_15D1C8+MINI_SYS_DEST_ROOM,&destination,2);
    D_8015C5C8_15D1C8[MINI_SYS_MARKER]=marker;
}
static unsigned short destination_room(void)
{
    unsigned short value;memcpy(&value,D_8015C5C8_15D1C8+MINI_SYS_DEST_ROOM,2);return value;
}
static void reset_return_fixture_character(unsigned short mini,int source_character)
{
    loaded=0;anchor_minigame_invites_update();loaded=1;
    player_task[0x60]=(unsigned char)source_character;
    D_8015C5D8_15D1D8[1]=(unsigned int)source_character;
    player_work[0x69]=0;live_world=1;prompt_safe=warp_ready=1;
    return_destination_calls=return_step_calls=0;
    D_8006B780_6C380[0x42*5+3]=-512;
    D_8006B780_6C380[0x42*5+4]=2048;
    anchor_minigame_invites_begin_load();D_800C7AB2=0x42;
    anchor_minigame_invites_finish_load();set_linked_player();
    set_player_position(12.25f,-8.5f,32766.75f);
    anchor_minigame_invites_note_transfer(mini);
    anchor_minigame_invites_begin_load();D_800C7AB2=mini;
    anchor_minigame_invites_finish_load();
    assert(D_8015C5D8_15D1D8[1]==(unsigned int)(game_for_room(mini)-1));
    player_task[0x60]=(unsigned char)(game_for_room(mini)-1);
    sync_game=game_for_room(mini);sync_returning=1;
}
static void reset_return_fixture(unsigned short mini)
{
    reset_return_fixture_character(mini,3);
}
static void invoke_return(unsigned short mini)
{
    if(mini==0x1e1) anchor_minigame_invites_return_ebisumaru();
    else anchor_minigame_invites_return_saved();
}
static void test_return_fields(void)
{
    static unsigned char before[sizeof(system_words)];
    static const unsigned short minis[]={0x1e0,0x1e1,0x1e2};
    for(int i=0;i<3;++i) {
        unsigned short mini=minis[i];reset_return_fixture(mini);
        native_destination(mini==0x1e1 ? 0x16d:0x60,mini==0x1e1 ? 9:4);
        memcpy(before,D_8015C5C8_15D1C8,sizeof(before));invoke_return(mini);
        assert(destination_room()==0x42 && s_return.phase==1);
        /* The full world loader selects the character bank in 8001FA80
         * before 8020D6BC returns or the player constructor runs. */
        assert(D_8015C5D8_15D1D8[1]==3);
        assert(D_8015C5D8_15D1D8[0x2c/4]==1);
        /* The helper runs inside the main frame. Its RETURN must keep the
         * later 80002040 RETURN update from rebinding the retiring player. */
        int swaps_before=swaps;
        anchor_minigame_invites_update();
        assert(D_8015C5D8_15D1D8[1]==3 && swaps==swaps_before);
        assert(published_game==0);
        assert(anchor_minigame_invites_reporting_character(mini)==-2);
        assert(!anchor_minigame_invites_character_motion_ready(mini));
        short xyz[3];memcpy(xyz,D_8015C5C8_15D1C8+MINI_SYS_DEST_X,sizeof(xyz));
        assert(xyz[0]==12 && xyz[1]==-8 && xyz[2]==32766);
        assert(return_destination_calls==1 && return_step_calls==1);
        assert(return_camera==-512 && return_player_rot==2048);
        assert(D_8015C5C8_15D1C8[0x3add4]==12);
        assert(D_8015C5C8_15D1C8[MINI_SYS_MARKER]==1);
        /* Ordinary warp destination/rotations/setup and main step change;
         * native saved backups, inventory/stats and result cleanup survive. */
        for(unsigned int j=0;j<sizeof(before);++j)
            if(!(j>=MINI_SYS_DEST_ROOM && j<MINI_SYS_DEST_ROOM+12) &&
               j!=0x3add4 && j!=MINI_SYS_MARKER &&
               !(j>=0x3ae2c && j<0x3ae32) &&
               !(j>=0x3ae3a && j<0x3ae40))
                assert(before[j]==D_8015C5C8_15D1C8[j]);
        set_player_position(99.f,99.f,99.f);
        anchor_minigame_invites_restore_spawn();assert(*(float *)(player_object+8)==99.f);
        anchor_minigame_invites_begin_load();D_800C7AB2=0x42;
        anchor_minigame_invites_finish_load();
        assert(D_8015C5D8_15D1D8[1]==3 && s_return.phase==2);
        unsigned short room;float saved[3];assert(!anchor_minigame_invites_source_waypoint(&room,saved));
        set_linked_player();anchor_minigame_invites_restore_spawn();
        float actual[3];memcpy(actual,player_object+8,sizeof(actual));
        assert(actual[0]==12.25f && actual[1]==-8.5f && actual[2]==32766.75f);
        assert(!s_return.phase);set_player_position(77.f,88.f,99.f);
        anchor_minigame_invites_restore_spawn();assert(*(float *)(player_object+8)==77.f);
    }
}

/* Exercise the hook ordering around the verified full-load request: the main
 * frame RETURN update precedes the next step dispatch, and the world loader
 * reads the character before 8020D6BC returns. This host fixture observes the
 * request and ordering; it does not execute the native asset loader. */
static void test_full_return_dispatch(void)
{
    static const unsigned short minis[]={0x1e0,0x1e1,0x1e2};
    for(int i=0;i<3;++i) for(int same_character=0;same_character<2;++same_character) {
        unsigned short mini=minis[i];int source_character=same_character?i:3;
        reset_return_fixture_character(mini,source_character);
        anchor_minigame_invites_update();
        assert(published_game==i+1);
        native_destination(mini==0x1e1?0x16d:0x60,mini==0x1e1?9:4);
        D_8015C5C8_15D1C8[0x3add4]=3;
        unsigned int extended_setup=0xa0;
        unsigned short extended_option=0x1234;
        memcpy(D_8015C5C8_15D1C8+0x3ae2c,&extended_setup,4);
        memcpy(D_8015C5C8_15D1C8+0x3ae30,&extended_option,2);
        int swaps_before=swaps;
        invoke_return(mini);
        assert(D_800C7AB2==mini); /* Only the native consumer commits room. */
        assert(D_8015C5C8_15D1C8[0x3add4]==12);
        assert(s_return.phase==1 && !s_room_loaded && !s_announced_visit);
        int requests=return_step_calls;
        invoke_return(mini);assert(return_step_calls==requests && requests==1);
        anchor_minigame_invites_update();
        assert(swaps==swaps_before && published_game==0);
        assert(anchor_minigame_invites_reporting_character(mini)==-2);
        assert(!anchor_minigame_invites_character_motion_ready(mini));
        memcpy(&extended_setup,D_8015C5C8_15D1C8+0x3ae2c,4);
        memcpy(&extended_option,D_8015C5C8_15D1C8+0x3ae30,2);
        assert(!extended_setup && !extended_option);

        int selected_character_bank=(int)D_8015C5D8_15D1D8[1];
        assert(D_8015C5C8_15D1C8[0x3add4]==12);
        assert(selected_character_bank==source_character);
        anchor_minigame_invites_begin_load();D_800C7AB2=0x42;
        anchor_minigame_invites_finish_load();
        set_linked_player();player_task[0x60]=(unsigned char)selected_character_bank;
        set_player_position(12.f,-8.f,32766.f);
        anchor_minigame_invites_restore_spawn();
        float restored[3];memcpy(restored,player_object+8,sizeof(restored));
        assert(restored[0]==12.25f && restored[1]==-8.5f && restored[2]==32766.75f);
        assert(!s_return.phase);
        anchor_minigame_invites_update();assert(swaps==swaps_before);
    }
}
static void test_return_guards(void)
{
    reset_return_fixture(0x1e2);native_destination(0x60,4);
    sync_returning=0;invoke_return(0x1e2);assert(destination_room()==0x60);
    sync_returning=1;sync_game=1;invoke_return(0x1e2);assert(destination_room()==0x60);sync_game=3;
    native_destination(0x60,1);invoke_return(0x1e2);assert(destination_room()==0x60);
    native_destination(0x60,4);s_source_mini_visit--;
    invoke_return(0x1e2);assert(destination_room()==0x60);s_source_mini_visit++;
    D_800C7AB2=0x40;invoke_return(0x1e2);assert(destination_room()==0x60);D_800C7AB2=0x1e2;
    s_source_valid=0;invoke_return(0x1e2);assert(destination_room()==0x60);
    assert(return_destination_calls==0 && return_step_calls==0);
    assert(D_8015C5D8_15D1D8[1]==2); /* Rejected exits cannot seed source char. */
    reset_return_fixture(0x1e1);native_destination(0x1e1,9);
    invoke_return(0x1e1);assert(destination_room()==0x1e1); /* Entry helper is not exit. */
    native_destination(0x16d,4);invoke_return(0x1e1);assert(destination_room()==0x16d);
    native_destination(0x16d,1);invoke_return(0x1e1);assert(destination_room()==0x16d);
    native_destination(0x16d,9);anchor_minigame_invites_return_saved();assert(destination_room()==0x16d);
    assert(return_destination_calls==0 && return_step_calls==0);
    assert(D_8015C5D8_15D1D8[1]==1);
    invoke_return(0x1e1);assert(s_return.phase==1);
    anchor_minigame_invites_begin_load();D_800C7AB2=0x43;anchor_minigame_invites_finish_load();
    assert(!s_return.phase);set_player_position(1.f,2.f,3.f);
    anchor_minigame_invites_restore_spawn();assert(*(float *)(player_object+8)==1.f);
    reset_return_fixture(0x1e0);native_destination(0x60,4);invoke_return(0x1e0);
    anchor_minigame_invites_begin_load();D_800C7AB2=0x42;anchor_minigame_invites_finish_load();
    D_801FC604_5B8514=0;anchor_minigame_invites_restore_spawn();assert(!s_return.phase);
    set_linked_player();
    reset_return_fixture(0x1e0);native_destination(0x60,4);invoke_return(0x1e0);
    loaded=0;anchor_minigame_invites_update();assert(!s_return.phase && !s_source_valid);loaded=1;
    reset_return_fixture(0x1e0);native_destination(0x60,4);invoke_return(0x1e0);
    anchor_minigame_invites_begin_load();D_800C7AB2=0x42;anchor_minigame_invites_finish_load();
    anchor_minigame_invites_begin_load();anchor_minigame_invites_finish_load();
    assert(!s_return.phase); /* A second visit cannot consume the first spawn. */
}
static void test_retained_player_return(void)
{
    reset_return_fixture(0x1e2);native_destination(0x60,4);invoke_return(0x1e2);
    anchor_minigame_invites_begin_load();D_800C7AB2=0x42;anchor_minigame_invites_finish_load();
    set_linked_player();void *work=player_work;
    memcpy(player_task+0x5c,&work,sizeof(work));
    set_player_position(12.f,-10.f,32766.f);
    anchor_minigame_invites_restore_in_place();
    float current_xyz[3],previous_xyz[3];
    memcpy(current_xyz,player_object+8,sizeof(current_xyz));
    memcpy(previous_xyz,player_work+0x5c,sizeof(previous_xyz));
    assert(current_xyz[0]==12.25f && current_xyz[1]==-8.5f && current_xyz[2]==32766.75f);
    assert(!memcmp(current_xyz,previous_xyz,sizeof(current_xyz)) && !s_return.phase);
}
static void test_yae_cleanup_source_return(void)
{
    static unsigned char before[sizeof(system_words)];
    for (int same_room = 0; same_room < 2; ++same_room)
        for (int outcome = 0; outcome < 2; ++outcome) {
            unsigned short source = same_room ? 0x154 : 0x42;
            int character = same_room ? 1 : 2;
            reset_yae_source(source, character);
            set_player_position(12.25f, -8.5f, 32766.75f);
            return_destination_calls = return_step_calls = 0;
            D_8006B780_6C380[source * 5 + 3] = -512;
            D_8006B780_6C380[source * 5 + 4] = 8;
            assert(anchor_minigame_invites_debug_start_yae());
            assert(D_8015C5D8_15D1D8[1] == 3);
            load_room(0x154);anchor_minigame_invites_yae_started();
            /* Success remains in the world; failure has queued Kegon's native
             * return. The adapter invokes finished only after either cleanup. */
            native_destination(0x154, outcome ? 9 : 0);
            memcpy(before, D_8015C5C8_15D1C8, sizeof(before));
            anchor_minigame_invites_return_saved();
            anchor_minigame_invites_return_ebisumaru();
            assert(return_destination_calls == 0);
            anchor_minigame_invites_yae_finished();
            assert(destination_room() == source && s_return.phase == 1);
            assert(return_destination_calls == 1 && return_step_calls == 1);
            assert(D_8015C5D8_15D1D8[1] == (unsigned int)character);
            assert(return_camera == -512 && return_player_rot == 8);
            for (unsigned int j = 0; j < sizeof(before); ++j)
                if (!(j >= MINI_SYS_DEST_ROOM && j < MINI_SYS_DEST_ROOM + 12) &&
                    j != 0x3add4 && j != MINI_SYS_MARKER &&
                    !(j >= 0x3ae2c && j < 0x3ae32) &&
                    !(j >= 0x3ae3a && j < 0x3ae40))
                    assert(before[j] == D_8015C5C8_15D1C8[j]);
            load_room(source);set_linked_player();
            assert(anchor_minigame_invites_required_character() == -1);
            set_player_position(100.f, 100.f, 100.f);
            anchor_minigame_invites_restore_spawn();
            float actual[3];memcpy(actual, player_object + 8, sizeof(actual));
            assert(actual[0] == 12.25f && actual[1] == -8.5f && actual[2] == 32766.75f);
            assert(!s_return.phase && D_8015C5D8_15D1D8[1] == (unsigned int)character);
            set_player_position(10.f, 10.f, 10.f);
            anchor_minigame_invites_restore_spawn();
            assert(*(float *)(player_object + 8) == 10.f);
        }
    reset_yae_source(0x154, 3);
    anchor_minigame_invites_yae_started();
    native_destination(0x154, 9);return_destination_calls = return_step_calls = 0;
    anchor_minigame_invites_yae_finished();
    assert(destination_room() == 0x154 && return_destination_calls == 0);
    assert(!s_yae_active && !s_source_valid);
}

int main(void)
{
    invites_regression_main();test_full_return_dispatch();test_return_fields();test_return_guards();test_retained_player_return();test_yae_cleanup_source_return();
    puts("minigame native returns: full warp request/order, preloader character seed, cleared camera overrides, exact XYZ and provenance guards passed");
    return 0;
}
