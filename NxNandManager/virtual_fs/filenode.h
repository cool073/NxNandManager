/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2019 Adrien J. <liryna.stark@gmail.com>
  Copyright (C) 2020 Google, Inc.
  Copyright (C) 2021 eliboa (eliboa@gmail.com)

  http://dokan-dev.github.io

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef FILENODE_H_
#define FILENODE_H_

#include <dokan/dokan.h>
#include <dokan/fileinfo.h>

#include "virtual_fs_helper.h"

#include <WinBase.h>
#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <set>
#include <memory>
#include <vector>

#ifndef byte
    typedef unsigned char byte;
#endif
namespace virtual_fs {

struct security_informations : std::mutex {
  std::unique_ptr<byte[]> descriptor = nullptr;
  DWORD descriptor_size = 0;

  security_informations() = default;
  security_informations(const security_informations &) = delete;
  security_informations &operator=(const security_informations &) = delete;

  void SetDescriptor(PSECURITY_DESCRIPTOR securitydescriptor) {
    if (!securitydescriptor) return;
    descriptor_size = GetSecurityDescriptorLength(securitydescriptor);
    //descriptor = boost::make_unique<byte[]>(descriptor_size);
    descriptor = std::unique_ptr<byte[]>(new byte[descriptor_size]);
    memcpy(descriptor.get(), securitydescriptor, descriptor_size);
  }
};

struct filetimes {
  void reset() {
    lastaccess = lastwrite = creation = get_currenttime();
  }

  static bool empty(const FILETIME* filetime) {
    return filetime->dwHighDateTime == 0 && filetime->dwLowDateTime == 0;
  }

  static LONGLONG get_currenttime() {
    FILETIME t;
    GetSystemTimeAsFileTime(&t);
    return virtual_fs_helper::DDwLowHighToLlong(t.dwLowDateTime, t.dwHighDateTime);
  }

  void set(LONGLONG t_creation, LONGLONG t_lastaccess, LONGLONG t_lastwrite) {
      creation = t_creation;
      lastaccess = t_lastaccess;
      lastwrite = t_lastwrite;
  }

  std::atomic<LONGLONG> creation;
  std::atomic<LONGLONG> lastaccess;
  std::atomic<LONGLONG> lastwrite;
};

// virtual_fs file context
// Each file/directory on the virtual_fs has his own filenode instance
// Alternated streams are also filenode where the main stream \myfile::$DATA
// has all the alternated streams (e.g. \myfile:foo:$DATA) attached to him
// and the alternated has main_stream assigned to the main stream filenode.
class filenode {
 public:
    filenode(const std::wstring &filename, bool is_directory, DWORD file_attr,
           const PDOKAN_IO_SECURITY_CONTEXT security_context);

    filenode(const filenode& f) = delete;

    DWORD read(LPVOID buffer, DWORD bufferlength, LONGLONG offset);
    DWORD write(LPCVOID buffer, DWORD number_of_bytes_to_write, LONGLONG offset);

    const LONGLONG get_filesize();
    void set_endoffile(const LONGLONG& byte_offset);

    // Filename can during a move so we need to protect it behind a lock
    const std::wstring get_filename();
    void set_filename(const std::wstring& filename);

    // Alternated streams

    void add_stream(const std::shared_ptr<filenode>& stream);
    void remove_stream(const std::shared_ptr<filenode>& stream);
    std::unordered_map<std::wstring, std::shared_ptr<filenode> > get_streams();

    // No lock needed above
    bool is_directory;
    DWORD attributes;
    LONGLONG fileindex = 0;
    LONGLONG size = 0;
    std::shared_ptr<filenode> main_stream;

    filetimes times;
    security_informations security;

 private:
    filenode() = default;

    std::mutex _data_mutex;
    // _data_mutex need to be aquired

    std::vector<uint8_t> _data;
    std::unordered_map<std::wstring, std::shared_ptr<filenode> > _streams;

    std::mutex _fileName_mutex;
    // _fileName_mutex need to be aquired
    std::wstring _fileName;
};
}  // namespace virtual_fs

#endif  // FILENODE_H_
