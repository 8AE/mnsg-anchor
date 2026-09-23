/* Included by test_world_dynamic_native.c: File_46 wave native substitutes and
 * a scheduler/registry/receipt lifecycle exercise. */
unsigned short D_8015CDB4, D_8015CDC0;
void *D_801FC60C_5B851C;
static unsigned int wave_target_object[32];
static unsigned int wave_model_12d[3], wave_model_fa[3];
static unsigned short wave_files_12d[2], wave_files_fa[2];
static unsigned int wave_clips_12d[2], wave_clips_fa[2];
static unsigned int wave_texture_table[2], wave_texture_command[2];
static int wave_alloc_fail, wave_births, wave_visuals, wave_native_updates;
static int wave_native_initializers;
static int wave_next_role;

void func_0800370C_70480C(void *a, void *o) {
  (void)o;
  S(a,0x8a)=512;
  DPTR(a,0xc)=(void *)func_0800376C_70486C;
}
void func_0800376C_70486C(void *a, void *o) {
  (void)o;
  short old=S(a,0x8a);
  S(a,0x8a)=old-1;
  if (!old) DPTR(a,0xc)=(void *)func_080037B0_7048B0;
}
void func_080037B0_7048B0(void *a, void *o) {
  (void)o;
  ++wave_native_updates;
  if (wave_alloc_fail || D_8015CDC0 || D_8015CDB4>=8) return;
  DynamicCallback init=wave_next_role==WW_MODEL_FA ?
      func_08003D64_704E64 : func_08003B4C_704C4C;
  void *child=func_802171A8_5D2678(a,init,WW_CHILD_CATEGORY);
  if (!child) return;
  H(child,0x5c)=WW_ENTITY;
  H(child,0x28)=WW_FILE_CODE;
  DPTR(child,0x84)=wave_target_object;
  world_dynamic_child(a,child);
  F(DPTR(child,0x18),8)=25.0f;
  F(DPTR(child,0x18),0xc)=50.0f;
  F(DPTR(child,0x18),0x10)=75.0f;
  ++D_8015CDB4;++wave_births;
}
void func_08003B4C_704C4C(void *a, void *o) {
  assert(o==DPTR(a,0x18));
  ++wave_native_initializers;
  H(a,0x5e)=WW_12D_MODEL;W(a,0x60)=0x022007e3u;
  W(a,0x64)|=0x8000u;
  B(a,0x8e)=1;B(a,0x8f)=2;H(a,0x96)=WW_CHILD_MASK;
  B(a,0x4c)=1;
  world_dynamic_animation(a,0);
  H(o,0x7e)=WW_CHILD_RATE;B(o,0x7c)=1;
  func_80224ABC_5DFF8C(a,0,1.f,1);
  B(a,0x98)=2;B(a,0x99)=10;B(a,0x9a)=10;B(a,0x9b)=5;
  H(a,0x3c)=WW_CHILD_RADIUS;H(a,0x3e)=WW_CHILD_HEIGHT;
  S(a,0x40)=WW_CHILD_OFFSET;
  W(a,0x48)=0xffffffffu;H(a,0x4e)=1;H(a,0x50)=10;S(a,0x52)=0;
  S(a,0x8a)=WW_12D_TIMER;
  F(o,0x10)=F(wave_target_object,0x10)+256.f;
  DPTR(a,0xc)=(void *)func_08003C90_704D90;
}
void func_08003D64_704E64(void *a, void *o) {
  ++wave_native_initializers;
  H(a,0x5e)=WW_FA_MODEL;W(a,0x60)=0x022007e3u;
  W(a,0x64)|=0x8000u;H(a,0x96)=WW_CHILD_MASK;
  world_dynamic_animation(a,0);
  H(o,0x7e)=WW_CHILD_RATE;B(o,0x7c)=1;
  B(a,0x98)=2;B(a,0x99)=10;B(a,0x9a)=10;B(a,0x9b)=5;
  H(a,0x3c)=WW_CHILD_RADIUS;H(a,0x3e)=WW_CHILD_HEIGHT;
  S(a,0x40)=WW_CHILD_OFFSET;
  W(a,0x48)=0xffffffffu;H(a,0x4e)=1;H(a,0x50)=10;S(a,0x52)=0;
  B(a,0x4c)=1;S(a,0x8a)=64;
  DPTR(a,0xc)=(void *)func_08003E60_704F60;
}
int func_08003A30_704B30(void *a) {
  if (!D_8015CDC0 && !(W(a,0x68)&0x80u)) return 0;
  ++wave_visuals;W(a,0x68)|=2u;
  if (D_8015CDB4) --D_8015CDB4;
  return 1;
}
void func_80212088_5CD558(void *a, void *o) { (void)a;(void)o; }
void func_80219E08_5D52D8(void *a,float scale) {
  void *o=DPTR(a,0x18);
  F(o,0x1c)*=scale;F(o,0x20)*=scale;F(o,0x24)*=scale;
}
void func_08003C90_704D90(void *a, void *o) {
  (void)o;
  ++wave_native_updates;
  if (func_08003A30_704B30(a)) return;
  S(a,0x8a)--;F(a,0x80)=WW_FLIGHT_VZ;
  if (S(a,0x8a)==-1) {
    ++wave_visuals;
    if (D_8015CDB4) --D_8015CDB4;
    W(a,0x68)|=2u;
    world_dynamic_delete();
  }
}
void func_08003E60_704F60(void *a, void *o) {
  (void)o;++wave_native_updates;
  if (func_08003A30_704B30(a)) return;
  if (S(a,0x8a)--==0) { S(a,0x8a)=80;DPTR(a,0xc)=(void *)func_08003F68_705068; }
}
void func_08003F68_705068(void *a, void *o) {
  (void)o;++wave_native_updates;
  if (func_08003A30_704B30(a)) return;
  if (S(a,0x8a)--==0) { S(a,0x8a)=32;DPTR(a,0xc)=(void *)func_08004034_705134; }
}
void func_08004034_705134(void *a, void *o) {
  (void)o;++wave_native_updates;
  if (func_08003A30_704B30(a)) return;
  if (S(a,0x8a)--==0) { S(a,0x8a)=64;DPTR(a,0xc)=(void *)func_08003F68_705068; }
}

static void wave_offer(DynamicActor *d,int owner,int receipt) {
  assert(capture(d));
  memcpy(d->net,d->row,sizeof(d->row));
  d->net[WW_INSTANCE]=(int)d->serial;
  d->net[WW_RECEIPT]=receipt;
  d->net[WD_OWNER]=owner;
  d->owner=(unsigned int)owner;
  d->dirty=d->have=1;
}
static void wave_prepare(void) {
  fixture();
  D_800C7AB2=d_room=WW_ROOM;
  D_8015CDB4=D_8015CDC0=0;
  wave_alloc_fail=wave_births=wave_visuals=wave_native_updates=0;
  wave_native_initializers=0;
  wave_next_role=WW_MODEL_12D;
  DPTR(actors[7],0x18)=objects[7];
  DPTR(actors[6],0x18)=objects[6];
  H(actors[6],0x5c)=H(actors[6],0x5e)=WW_ENTITY;
  B(actors[6],0x74)=3;
  F(wave_target_object,8)=100.f;
  F(wave_target_object,0xc)=20.f;
  F(wave_target_object,0x10)=200.f;
  D_801FC60C_5B851C=wave_target_object;
  DPTR(actors[7],0x18)=wave_target_object;
  DPTR(wave_model_12d,0)=wave_files_12d;
  DPTR(wave_model_12d,4)=wave_clips_12d;
  DPTR(wave_model_12d,8)=wave_texture_table;
  DPTR(wave_texture_table,0)=wave_texture_command;
  W(wave_texture_command,0)=0x08002890u;
  B(wave_texture_command,4)=60;
  DPTR(wave_model_fa,0)=wave_files_fa;
  DPTR(wave_model_fa,4)=wave_clips_fa;
  wave_files_12d[0]=0x19e;wave_files_12d[1]=0;
  wave_files_fa[0]=0x1b6;wave_files_fa[1]=0;
  wave_clips_12d[0]=0x08000130u;
  wave_clips_fa[0]=0x080003e4u;
  D_80236984_5F1E54[WW_12D_MODEL]=wave_model_12d;
  D_80236984_5F1E54[WW_FA_MODEL]=wave_model_fa;
}
void wave_lifecycle_test(void) {
  DynamicActor *root,*head,*bee,*copy;
  void *a;
  int r[WORLD_DYNAMIC_WORDS],before;
  wave_prepare();
  a=func_802171A8_5D2678(actors[6],func_0800370C_70480C,WW_ROOT_CATEGORY);
  assert(last_spawn_category==WW_ROOT_CATEGORY);
  H(a,0x5c)=H(a,0x5e)=WW_ENTITY;H(a,0x28)=WW_FILE_CODE;
  world_dynamic_child(actors[6],a);
  world_dynamic_wave_root(a);
  root=lookup(a);assert(root && root->kind==WD_WAVE && !root->row[WW_ROLE]);
  D_8016DAB4_16E6B4=a;
  func_0800370C_70480C(a,DPTR(a,0x18));
  world_dynamic_post(a);
  assert(capture(root) && anchor_world_wave_valid(root->row));
  assert(root->row[WD_TIMER]==512 && root->row[WD_PARENT]==WW_ROOT_PARENT);
  S(a,0x8a)=0;wave_offer(root,2,1);tick(root);
  assert(root->row[WD_PHASE]==WW_PRODUCER);
  wave_alloc_fail=1;wave_offer(root,2,2);tick(root);
  assert(!root->row[WD_ORDINAL] && !wave_births);
  wave_alloc_fail=0;tick(root);
  assert(root->row[WD_ORDINAL]==1 && wave_births==1 && D_8015CDB4==1);
  a=actors[1];head=lookup(a);assert(head && head->wave_birth);
  D_8016DAB4_16E6B4=a;
  world_dynamic_wave_child_12d(a);
  func_08003B4C_704C4C(a,DPTR(a,0x18));
  world_dynamic_post(a);
  assert(head->kind==WD_WAVE && head->wave_initialized && capture(head));
  assert(head->row[WW_ROLE]==WW_MODEL_12D && head->row[WD_ORDINAL]==1);
  assert(head->row[WW_TARGET_X]==10000 && head->row[WW_TARGET_Z]==20000);
  assert(anchor_world_wave_valid(head->row));
  assert(wave_resource(head->row));
  B(wave_texture_command,4)=0;assert(!wave_resource(head->row));
  B(wave_texture_command,4)=60;
  /* Proximity reload preallocates a generic task record before 0370C runs.
   * Rebind the retained producer without replaying the 512-tick wait. */
  world_dynamic_reuse(root->actor);
  assert(!root->actor && root->row[WD_PHASE]==WW_PRODUCER &&
         root->row[WD_ORDINAL]==1);
  a=func_802171A8_5D2678(actors[6],func_0800370C_70480C,WW_ROOT_CATEGORY);
  H(a,0x5c)=H(a,0x5e)=WW_ENTITY;H(a,0x28)=WW_FILE_CODE;
  world_dynamic_child(actors[6],a);
  world_dynamic_wave_root(a);
  assert(lookup(a)==root && root->row[WD_PHASE]==WW_PRODUCER &&
         root->row[WD_ORDINAL]==1 && !root->row[WW_RECEIPT]);
  D_8016DAB4_16E6B4=a;
  func_0800370C_70480C(a,DPTR(a,0x18));world_dynamic_post(a);
  wave_alloc_fail=1;wave_offer(root,2,9);
  before=wave_native_updates;tick(root);
  assert(wave_native_updates==before+1 && root->row[WD_ORDINAL]==1);
  wave_alloc_fail=0;
  wave_offer(head,1,3);before=wave_native_updates;tick(head);
  assert(wave_native_updates==before && !(W(head->actor,0x68)&2u));
  wave_offer(head,2,4);tick(head);
  assert(wave_native_updates>before);
  /* A remote copy binds only in its own scheduler slot, then waits for a
   * current-instance receipt before it can move or run native pursuit. */
  memcpy(r,head->row,sizeof(r));r[WD_LIFE]=WD_LIVE;
  r[WD_OWNER]=1;r[WD_CID]=WW_CHILD_ORIGIN;r[WD_SERIAL]=9;
  r[WW_INSTANCE]=r[WW_RECEIPT]=0;
  copy=reconstruct(r);assert(copy && copy->proxy && !copy->wave_initialized);
  assert(last_spawn_category==WW_CHILD_CATEGORY);
  copy->have=1;copy->owner=1;
  before=wave_native_initializers;tick(copy);
  assert(copy->wave_initialized && !copy->row[WW_RECEIPT]);
  assert(wave_native_initializers==before); /* no replayed native birth VFX */
  memcpy(copy->net,r,sizeof(r));copy->net[WW_INSTANCE]=(int)copy->serial;
  copy->dirty=1;assert(!apply(copy));
  copy->net[WW_RECEIPT]=5;copy->dirty=1;assert(apply(copy));
  assert(copy->row[WW_RECEIPT]==5 && W(copy->actor,0x60));
  copy->net[WD_OWNER]=2;copy->owner=2;copy->dirty=1;
  DPTR(copy->actor,0x84)=0;
  before=wave_native_updates;
  tick(copy);tick(copy);
  assert(wave_native_updates==before+2 && DPTR(copy->actor,0x84)==wave_target_object);
  assert(copy->native!= (void *)dynamic_callback);
  /* An accepted local hit is held before 18350 can run its lethal path. */
  W(copy->actor,0x68)|=0x40000u;
  world_dynamic_enemy_damage(copy->actor);
  assert(copy->claimed && copy->row[WD_LIFE]==WD_CLAIM);
  assert(!(W(copy->actor,0x68)&0x40080u));
  copy->row[WD_LIFE]=WD_REMOVED;copy->row[WD_LANDED]=1;
  tick(copy);assert(wave_visuals>0 && (W(copy->actor,0x68)&2u));
  anchor_world_dynamic_frame(WW_ROOM,0,1,0);
  assert(!d_active && DPTR(head->actor,0xc)==
         (void *)func_08003C90_704D90);
  /* Re-enter a clean room to cover 0xFA and synchronous native expiry. */
  wave_prepare();
  a=func_802171A8_5D2678(actors[6],func_0800370C_70480C,0);
  H(a,0x5c)=H(a,0x5e)=WW_ENTITY;H(a,0x28)=WW_FILE_CODE;
  world_dynamic_child(actors[6],a);world_dynamic_wave_root(a);
  root=lookup(a);D_8016DAB4_16E6B4=a;
  func_0800370C_70480C(a,DPTR(a,0x18));world_dynamic_post(a);
  S(a,0x8a)=0;wave_offer(root,2,6);tick(root);
  wave_next_role=WW_MODEL_FA;tick(root);
  bee=lookup(actors[1]);assert(bee && bee->wave_birth);
  D_8016DAB4_16E6B4=bee->actor;
  world_dynamic_wave_child_fa(bee->actor);
  func_08003D64_704E64(bee->actor,DPTR(bee->actor,0x18));
  world_dynamic_post(bee->actor);
  assert(capture(bee) && anchor_world_wave_valid(bee->row));
  assert(bee->row[WW_ROLE]==WW_MODEL_FA && bee->row[WD_ORDINAL]==1);
  wave_offer(bee,2,7);assert(apply(bee));S(bee->actor,0x8a)=0;tick(bee);
  assert(bee->row[WD_PHASE]==WW_COAST);
  /* Native 12D old-zero deletion calls reuse before returning. The tombstone
   * already exists and must distinguish expiry from a committed kill. */
  wave_next_role=WW_MODEL_12D;tick(root);
  head=lookup(actors[2]);assert(head && head->wave_birth);
  D_8016DAB4_16E6B4=head->actor;
  world_dynamic_wave_child_12d(head->actor);
  func_08003B4C_704C4C(head->actor,DPTR(head->actor,0x18));
  world_dynamic_post(head->actor);wave_offer(head,2,8);
  assert(apply(head));S(head->actor,0x8a)=0;tick(head);
  assert(head->row[WD_LIFE]==WD_REMOVED && !head->row[WD_LANDED]);
  assert(head->row[WD_COMMITTER]==2 && wave_visuals==1);
  D_8015CDC0=1;tick(bee);
  assert(bee->claimed && bee->row[WD_LIFE]==WD_CLAIM);
  assert(capture(root) && root->row[WW_STOP]);
  D_8015CDC0=0;wave_offer(root,1,9);
  assert(apply(root) && D_8015CDC0==1);
}
