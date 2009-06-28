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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#ifndef _D_FILE_ENTRY_H_
#define _D_FILE_ENTRY_H_

#include "common.h"

#include <string>
#include <deque>
#include <ostream>

#include "SharedHandle.h"
#include "File.h"
#include "Request.h"

namespace aria2 {

class URISelector;

class FileEntry {
private:
  std::string path;
  std::deque<std::string> _uris;
  std::deque<std::string> _spentUris;
  uint64_t length;
  off_t offset;
  bool extracted;
  bool requested;
  std::deque<SharedHandle<Request> > _requestPool;
  std::deque<SharedHandle<Request> > _inFlightRequests;
  std::string _contentType;
public:
  FileEntry():length(0), offset(0), extracted(false), requested(false) {}

  FileEntry(const std::string& path, uint64_t length, off_t offset,
	    const std::deque<std::string>& uris = std::deque<std::string>());

  ~FileEntry();

  FileEntry& operator=(const FileEntry& entry);

  std::string getBasename() const
  {
    return File(path).getBasename();
  }

  std::string getDirname() const
  {
    return File(path).getDirname();
  }

  const std::string& getPath() const { return path; }

  void setPath(const std::string& path) { this->path = path; }

  uint64_t getLength() const { return length; }

  void setLength(uint64_t length) { this->length = length; }

  off_t getOffset() const { return offset; }

  void setOffset(off_t offset) { this->offset = offset; }

  off_t getLastOffset() { return offset+length; }

  bool isExtracted() const { return extracted; }

  void setExtracted(bool flag) { this->extracted = flag; }

  bool isRequested() const { return requested; }

  void setRequested(bool flag) { this->requested = flag; }

  void setupDir();

  // TODO1.5 remove this in favor of getRemainingUris()
  const std::deque<std::string>& getAssociatedUris() const
  {
    return _uris;
  }

  const std::deque<std::string>& getRemainingUris() const
  {
    return _uris;
  }

  const std::deque<std::string>& getSpentUris() const
  {
    return _spentUris;
  }

  void setUris(const std::deque<std::string>& uris)
  {
    _uris = uris;
  }

  // Inserts _uris and _spentUris into uris.
  void getUris(std::deque<std::string>& uris) const;

  void setContentType(const std::string& contentType)
  {
    _contentType = contentType;
  }

  const std::string& getContentType() const { return _contentType; }

  std::string selectUri(const SharedHandle<URISelector>& uriSelector);

  // If pooled Request object is available, one of them is removed
  // from the pool and returned.  If pool is empty, then select URI
  // using selectUri(selector) and construct Request object using it
  // and return the Request object.
  SharedHandle<Request> getRequest(const SharedHandle<URISelector>& selector);

  void poolRequest(const SharedHandle<Request>& request);

  bool removeRequest(const SharedHandle<Request>& request);

  size_t countInFlightRequest() const
  {
    return _inFlightRequests.size();
  }

  bool operator<(const FileEntry& fileEntry) const;

  bool exists() const;

  // Translate global offset goff to file local offset.
  off_t gtoloff(off_t goff) const;
};

typedef SharedHandle<FileEntry> FileEntryHandle;
typedef std::deque<FileEntryHandle> FileEntries;

// Returns the first FileEntry which isRequested() method returns
// true.  If no such FileEntry exists, then returns
// SharedHandle<FileEntry>().
template<typename InputIterator>
SharedHandle<FileEntry> getFirstRequestedFileEntry
(InputIterator first, InputIterator last)
{
  for(; first != last; ++first) {
    if((*first)->isRequested()) {
      return *first;
    }
  }
  return SharedHandle<FileEntry>();
}

// Counts the number of files selected in the given iterator range
// [first, last).
template<typename InputIterator>
size_t countRequestedFileEntry(InputIterator first, InputIterator last)
{
  size_t count = 0;
  for(; first != last; ++first) {
    if((*first)->isRequested()) {
      ++count;
    }
  }
  return count;
}

// Writes first filename to given o.  If memory is true, the output is
// "[MEMORY]" plus the basename of the first filename.  If there is no
// FileEntry, writes "n/a" to o.  If more than 1 FileEntry are in the
// iterator range [first, last), "(Nmore)" is written at the end where
// N is the number of files in iterator range [first, last) minus 1.
template<typename InputIterator>
void writeFilePath
(InputIterator first, InputIterator last, std::ostream& o, bool memory)
{
  SharedHandle<FileEntry> e = getFirstRequestedFileEntry(first, last);
  if(e.isNull()) {
    o << "n/a";
  } else {
    if(e->getPath().empty()) {
      o << "n/a";
    } else {
      if(memory) {
	o << "[MEMORY]" << File(e->getPath()).getBasename();
      } else {
	o << e->getPath();
      }
    }
    size_t count = countRequestedFileEntry(first, last);
    if(count > 1) {
      o << " (" << count-1 << "more)";
    }
  }
}

}

#endif // _D_FILE_ENTRY_H_
