// clang-format off
#include <postgres.h>
#include <fmgr.h>
// clang-format on
#include <access/genam.h>
#include <catalog/indexing.h>
#include <commands/extension.h>
#include <executor/executor.h>
#include <lib/dshash.h>
#include <miscadmin.h>
#include <storage/lwlock.h>

#include "omni_common.h"
#if PG_MAJORVERSION_NUM >= 17
#include <utils/tuplestore.h>
#endif

void _Omni_init(const omni_handle *handle) {
  omni_hook alter_extension_hook = {.name = "extension upgrade",
                                    .type = omni_hook_process_utility,
                                    .fn = {.process_utility = extension_upgrade_hook},
                                    .wrap = true};
  handle->register_hook(handle, &alter_extension_hook);
}

PG_FUNCTION_INFO_V1(modules);
Datum modules(PG_FUNCTION_ARGS) {
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  rsinfo->returnMode = SFRM_Materialize;

  MemoryContext per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  MemoryContext oldcontext = MemoryContextSwitchTo(per_query_ctx);

  Tuplestorestate *tupstore = tuplestore_begin_heap(false, false, work_mem);
  rsinfo->setResult = tupstore;

  LWLockAcquire(&(locks + OMNI_LOCK_MODULE)->lock, LW_SHARED);

  dshash_seq_status status;
  dshash_seq_init(&status, omni_modules, false);

  ModuleEntry *entry;
  while ((entry = dshash_seq_next(&status)) != NULL) {
    dsa_area *entry_dsa = dsa_handle_to_area(entry->dsa);
    omni_handle_private *handle = dsa_get_address(entry_dsa, entry->pointer);
    Datum values[4] = {Int64GetDatum(entry->id), CStringGetDatum(entry->path),
                       Int16GetDatum(handle->magic.version), Int16GetDatum(handle->magic.revision)};
    bool isnull[4] = {false, false, false, false};
    tuplestore_putvalues(tupstore, rsinfo->expectedDesc, values, isnull);
  }

  dshash_seq_term(&status);
  LWLockRelease(&(locks + OMNI_LOCK_MODULE)->lock);

#if PG_MAJORVERSION_NUM < 17
  tuplestore_donestoring(tupstore);
#endif

  MemoryContextSwitchTo(oldcontext);
  PG_RETURN_NULL();
}

#include "hook_types.h"

PG_FUNCTION_INFO_V1(hooks);
Datum hooks(PG_FUNCTION_ARGS) {
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  rsinfo->returnMode = SFRM_Materialize;

  MemoryContext per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  MemoryContext oldcontext = MemoryContextSwitchTo(per_query_ctx);

  Tuplestorestate *tupstore = tuplestore_begin_heap(false, false, work_mem);
  rsinfo->setResult = tupstore;

  for (int type = 0; type < __OMNI_HOOK_TYPE_COUNT; type++) {
    for (int i = hook_entry_points.entry_points_count[type] - 1; i >= 0; i--) {
      hook_entry_point *hook = hook_entry_points.entry_points[type] + i;
      omni_handle_private *phandle =
          hook->handle ? struct_from_member(omni_handle_private, handle, hook->handle) : NULL;
      Datum values[4] = {CStringGetDatum(omni_hook_types[type]), CStringGetDatum(hook->name),
                         phandle ? Int64GetDatum(phandle->id) : Int64GetDatum(0),
                         Int32GetDatum(hook_entry_points.entry_points_count[type] - i)};
      bool isnull[4] = {omni_hook_types[type] == NULL, hook->name == NULL, hook->handle == NULL,
                        false};
      tuplestore_putvalues(tupstore, rsinfo->expectedDesc, values, isnull);
    }
  }

#if PG_MAJORVERSION_NUM < 17
  tuplestore_donestoring(tupstore);
#endif

  MemoryContextSwitchTo(oldcontext);
  PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(shmem_allocations);
Datum shmem_allocations(PG_FUNCTION_ARGS) {
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  rsinfo->returnMode = SFRM_Materialize;

  MemoryContext per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  MemoryContext oldcontext = MemoryContextSwitchTo(per_query_ctx);

  Tuplestorestate *tupstore = tuplestore_begin_heap(false, false, work_mem);
  rsinfo->setResult = tupstore;

  LWLockAcquire(&(locks + OMNI_LOCK_ALLOCATION)->lock, LW_SHARED);

  dshash_seq_status status;
  dshash_seq_init(&status, omni_allocations, false);

  ModuleAllocation *entry;
  while ((entry = dshash_seq_next(&status)) != NULL) {

    Datum values[4] = {CStringGetDatum(entry->key.name), Int64GetDatum(entry->key.module_id),
                       Int64GetDatum(entry->size),
                       Int32GetDatum(pg_atomic_read_u32(&entry->refcounter))};
    bool isnull[4] = {false, false, false, false};
    tuplestore_putvalues(tupstore, rsinfo->expectedDesc, values, isnull);
  }

  dshash_seq_term(&status);
  LWLockRelease(&(locks + OMNI_LOCK_ALLOCATION)->lock);

#if PG_MAJORVERSION_NUM < 17
  tuplestore_donestoring(tupstore);
#endif

  MemoryContextSwitchTo(oldcontext);
  PG_RETURN_NULL();
}
