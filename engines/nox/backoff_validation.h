/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_BACKOFF_VALIDATION_H
#define NOX_BACKOFF_VALIDATION_H
namespace Nox {
static bool validateBackoffEndpoint(Common::String &error) {
 auto fail=[&](const char *message){error=message;return false;};
 auto &ce=Common::CEAchievements();
 if(ConfMan.getActiveDomainName()!="nox-gameplay-test"||ce.getLinkToken().empty()||!ConfMan.get("ce_achievements_api_base_url",Common::ConfigManager::kApplicationDomain).hasPrefix("http://127.0.0.1:"))return fail("Backoff validation requires isolated loopback target");
 const int phase=ConfMan.getInt("nox_backoff_phase");
 const bool permanent=ConfMan.getBool("nox_backoff_permanent");
 if(phase<1||phase>3)return fail("Unknown backoff validation phase");
 if(phase==1&&!ce.startPlaythroughForTarget("nox-gameplay-test","nox-archaist","apple2-v119","Local backoff verification"))return fail("Could not queue actual session start");
 auto settle=[&](){const uint32 deadline=g_system->getMillis()+10000;while(ce.isSyncBusy()&&g_system->getMillis()<deadline)g_system->delayMillis(20);return !ce.isSyncBusy();};
 if(!settle())return fail("Loopback response did not finish");
 const uint32 pending=permanent||phase==3?0:1;
 if(phase==3){ce.retryQueuedEvents();if(!settle())return fail("Allowed retry did not finish");}
 else {
  // Repeated normal retry calls must not create network attempts during a wait,
  // including in a fresh process. The external endpoint enforces a hard budget.
  const uint32 end=g_system->getMillis()+1000;
  while(g_system->getMillis()<end){ce.retryQueuedEvents();if(ce.isSyncBusy())return fail("Blocked/permanent event was transmitted again");g_system->delayMillis(20);}
 }
 if(ce.isSyncBusy()||ce.getPendingEventCount()!=pending)return fail("Backoff changed pending/permanent/synced semantics");
 Common::OutSaveFile *out=g_system->getSavefileManager()->openForSaving(Common::String::format("nox-backoff-%d.tsv",phase),false);if(!out)return fail("Cannot persist backoff evidence");
 out->writeString(Common::String::format("PASS\tphase=%d\tpending=%u\tpermanent=%u\n",phase,pending,permanent?1:0));out->finalize();const bool ok=!out->err();delete out;return ok;
}
}
#endif
