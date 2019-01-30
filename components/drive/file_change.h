// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_FILE_CHANGE_H_
#define COMPONENTS_DRIVE_FILE_CHANGE_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"

namespace drive {
class ResourceEntry;

class FileChange {
 public:
  enum FileType {
    FILE_TYPE_NO_INFO,
    FILE_TYPE_FILE,
    FILE_TYPE_DIRECTORY,
  };

  enum ChangeType {
    CHANGE_TYPE_ADD_OR_UPDATE,
    CHANGE_TYPE_DELETE,
  };

  class Change {
   public:
    Change(ChangeType change, FileType file_type);
    Change(ChangeType change,
           FileType file_type,
           const std::string& team_drive_id);

    bool IsAddOrUpdate() const { return change_ == CHANGE_TYPE_ADD_OR_UPDATE; }
    bool IsDelete() const { return change_ == CHANGE_TYPE_DELETE; }

    bool IsFile() const { return file_type_ == FILE_TYPE_FILE; }
    bool IsDirectory() const { return file_type_ == FILE_TYPE_DIRECTORY; }
    bool IsTypeUnknown() const { return !IsFile() && !IsDirectory(); }

    ChangeType change() const { return change_; }
    FileType file_type() const { return file_type_; }
    const std::string& team_drive_id() const { return team_drive_id_; }

    std::string DebugString() const;

    bool operator==(const Change& that) const {
      return change() == that.change() && file_type() == that.file_type() &&
             team_drive_id() == that.team_drive_id();
    }

   private:
    ChangeType change_;
    FileType file_type_;
    // The team drive id, will be empty if the change is not a team drive root.
    std::string team_drive_id_;
  };

  class ChangeList {
   public:
    typedef base::circular_deque<Change> List;

    ChangeList();
    ChangeList(const ChangeList& other);
    ~ChangeList();

    // Updates the list with the |new_change|.
    void Update(const Change& new_change);

    size_t size() const { return list_.size(); }
    bool empty() const { return list_.empty(); }
    void clear() { list_.clear(); }
    const List& list() const { return list_; }
    const Change& front() const { return list_.front(); }
    const Change& back() const { return list_.back(); }

    ChangeList PopAndGetNewList() const;

    std::string DebugString() const;

   private:
    List list_;
  };

 public:
  typedef std::map<base::FilePath, FileChange::ChangeList> Map;

  FileChange();
  FileChange(const FileChange& other);
  ~FileChange();

  void Update(const base::FilePath file_path,
              const FileChange::Change& new_change);
  void Update(const base::FilePath file_path,
              const FileChange::ChangeList& list);
  void Update(const base::FilePath file_path,
              FileType type,
              FileChange::ChangeType change);
  void Update(const base::FilePath file_path,
              const ResourceEntry& entry,
              FileChange::ChangeType change);
  void Apply(const FileChange& new_changed_files);

  const Map& map() const { return map_; }

  size_t size() const { return map_.size(); }
  bool empty() const { return map_.empty(); }
  void ClearForTest() { map_.clear(); }
  size_t CountDirectory(const base::FilePath& directory_path) const;
  size_t count(const base::FilePath& file_path) const {
    return map_.count(file_path);
  }

  std::string DebugString() const;

 private:
  Map map_;
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_FILE_CHANGE_H_
