/*
   Copyright 2022 Alessandro Bozzato

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef VFS_H_
#define VFS_H_

#include "util.h"

/// VFS stands for Virtual File System
/// It is used to refer to files through FileId, an integer

/// FileId is an integer type used to refer to a file
typedef uint16_t FileId;

/// Add a file to the virtual file system
FileId add_file(char const *path, stringview content);

/// Get the filepath of the file referred by file_id
char const *filepath(FileId file_id);

/// Get content of the file referred by file_id
stringview filecontent(FileId file_id);

/// Cleanup the Virtual File System
void vfs_cleanup(void);

#endif // VFS_H_
