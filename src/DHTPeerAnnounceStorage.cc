/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "DHTPeerAnnounceStorage.h"

#include <cstring>
#include <algorithm>

#include "DHTPeerAnnounceEntry.h"
#include "Peer.h"
#include "DHTConstants.h"
#include "DHTTaskQueue.h"
#include "DHTTaskFactory.h"
#include "DHTTask.h"
#include "LogFactory.h"
#include "Logger.h"
#include "util.h"
#include "a2functional.h"
#include "wallclock.h"
#include "fmt.h"

namespace aria2 {

DHTPeerAnnounceStorage::DHTPeerAnnounceStorage() {}

DHTPeerAnnounceStorage::~DHTPeerAnnounceStorage() {}

namespace {
class InfoHashLess
{
public:
  bool operator()(const SharedHandle<DHTPeerAnnounceEntry>& lhs,
                  const SharedHandle<DHTPeerAnnounceEntry>& rhs)
  {
    return memcmp(lhs->getInfoHash(), rhs->getInfoHash(), DHT_ID_LENGTH) < 0;
  }
};
} // namespace

SharedHandle<DHTPeerAnnounceEntry>
DHTPeerAnnounceStorage::getPeerAnnounceEntry(const unsigned char* infoHash)
{
  SharedHandle<DHTPeerAnnounceEntry> entry(new DHTPeerAnnounceEntry(infoHash));

  std::deque<SharedHandle<DHTPeerAnnounceEntry> >::iterator i = 
    std::lower_bound(entries_.begin(), entries_.end(), entry, InfoHashLess());

  if(i != entries_.end() &&
     memcmp(infoHash, (*i)->getInfoHash(), DHT_ID_LENGTH) == 0) {
    entry = *i;
  } else {
    entries_.insert(i, entry);
  }
  return entry;
}

void
DHTPeerAnnounceStorage::addPeerAnnounce(const unsigned char* infoHash,
                                        const std::string& ipaddr, uint16_t port)
{
  A2_LOG_DEBUG(fmt("Adding %s:%u to peer announce list: infoHash=%s",
                   ipaddr.c_str(), port,
                   util::toHex(infoHash, DHT_ID_LENGTH).c_str()));
  getPeerAnnounceEntry(infoHash)->addPeerAddrEntry(PeerAddrEntry(ipaddr, port));
}

bool DHTPeerAnnounceStorage::contains(const unsigned char* infoHash) const
{
  SharedHandle<DHTPeerAnnounceEntry> entry(new DHTPeerAnnounceEntry(infoHash));
  return 
    std::binary_search(entries_.begin(), entries_.end(), entry, InfoHashLess());
}

void DHTPeerAnnounceStorage::getPeers(std::vector<SharedHandle<Peer> >& peers,
                                      const unsigned char* infoHash)
{
  SharedHandle<DHTPeerAnnounceEntry> entry(new DHTPeerAnnounceEntry(infoHash));

  std::deque<SharedHandle<DHTPeerAnnounceEntry> >::iterator i = 
    std::lower_bound(entries_.begin(), entries_.end(), entry, InfoHashLess());
  if(i != entries_.end() &&
     memcmp(infoHash, (*i)->getInfoHash(), DHT_ID_LENGTH) == 0 &&
     !(*i)->empty()) {
    (*i)->getPeers(peers);
  }
}

namespace {
class RemoveStalePeerAddrEntry
{
public:
  void operator()(const SharedHandle<DHTPeerAnnounceEntry>& e)
  {
    e->removeStalePeerAddrEntry(DHT_PEER_ANNOUNCE_PURGE_INTERVAL);
  }
};
} // namespace

void DHTPeerAnnounceStorage::handleTimeout()
{
  A2_LOG_DEBUG(fmt("Now purge peer announces(%lu entries) which are timed out.",
                   static_cast<unsigned long>(entries_.size())));
  std::for_each(entries_.begin(), entries_.end(), RemoveStalePeerAddrEntry());
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                mem_fun_sh(&DHTPeerAnnounceEntry::empty)),
                 entries_.end());
  A2_LOG_DEBUG(fmt("Currently %lu peer announce entries",
                   static_cast<unsigned long>(entries_.size())));
}

void DHTPeerAnnounceStorage::announcePeer()
{
  A2_LOG_DEBUG("Now announcing peer.");
  for(std::deque<SharedHandle<DHTPeerAnnounceEntry> >::iterator i =
        entries_.begin(), eoi = entries_.end(); i != eoi; ++i) {
    if((*i)->getLastUpdated().
       difference(global::wallclock()) >= DHT_PEER_ANNOUNCE_INTERVAL) {
      (*i)->notifyUpdate();
      SharedHandle<DHTTask> task =
        taskFactory_->createPeerAnnounceTask((*i)->getInfoHash());
      taskQueue_->addPeriodicTask2(task);
      A2_LOG_DEBUG
        (fmt("Added 1 peer announce: infoHash=%s",
             util::toHex((*i)->getInfoHash(), DHT_ID_LENGTH).c_str()));
    }
  }
}

void DHTPeerAnnounceStorage::setTaskQueue(const SharedHandle<DHTTaskQueue>& taskQueue)
{
  taskQueue_ = taskQueue;
}

void DHTPeerAnnounceStorage::setTaskFactory(const SharedHandle<DHTTaskFactory>& taskFactory)
{
  taskFactory_ = taskFactory;
}

} // namespace aria2
