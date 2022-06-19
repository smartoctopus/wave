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

#ifndef DIAGNOSTIC_H_
#define DIAGNOSTIC_H_

#include "util.h"
#include "vfs.h"

typedef struct Span {
    FileId file_id;
    uint32_t start;
    uint32_t end;
} Span;

typedef struct Diagnostic {
    Span location;
    bool is_error;
    char const *message;
    char const *label;
    char const *hint;
} Diagnostic;

alwaysinline Diagnostic error(Span span, char const *message, char const *label, char const *hint)
{
    if (hint != NULL) {
        hint = strf("%s", hint);
    }
    return (Diagnostic) {
        .location = span,
        .is_error = true,
        .message = strf("%s", message),
        .label = strf("%s", label),
        .hint = hint,
    };
}

alwaysinline Diagnostic warn(Span span, char const *message, char const *label, char const *hint)
{
    if (hint != NULL) {
        hint = strf("%s", hint);
    }
    return (Diagnostic) {
        .location = span,
        .is_error = false,
        .message = strf("%s", message),
        .label = strf("%s", label),
        .hint = hint,
    };
}

void emit_diagnostics(array(Diagnostic) diags);
void free_diagnostics(array(Diagnostic) * diags);

#endif // DIAGNOSTIC_H_
