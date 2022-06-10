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

#include "vfs.h"

typedef struct File {
    char const *path;
    stringview content;
} File;

static array(File) files = NULL;

FileId add_file(char const *path, stringview content)
{
    stringview file_content = { .length = content.length, .str = strf("%.*s", content.length, content.str) };
    File file = { .path = strf("%s", path), .content = file_content };
    array_push(files, file);
    return array_length(files) - 1;
}

char const *filepath(FileId file_id)
{
    if (file_id > array_length(files)) {
        return NULL;
    }
    return files[file_id].path;
}

stringview filecontent(FileId file_id)
{
    if (file_id > array_length(files)) {
        return (stringview) { 0 };
    }
    return files[file_id].content;
}

void vfs_cleanup(void)
{
    for (uint_fast16_t i = 0; i < array_length(files); ++i) {
        free((void *)files[i].content.str);
        free((void *)files[i].path);
    }
    array_free(files);
}
