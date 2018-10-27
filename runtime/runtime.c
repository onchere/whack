/**
 * Copyright 2018 Onchere Bironga
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// gcc runtime.c -o ../build/runtime.o -c
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32

DWORD __builtin_page_size() {
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  return sysInfo.dwPageSize;
}

/// Allocates an executable region of memory
void* __builtin_virtual_alloc() {
  return VirtualAlloc(NULL, __builtin_page_size(), MEM_COMMIT,
                      PAGE_EXECUTE_READWRITE);
}

/// Frees a region of memory allocated by __builtin_virtual_alloc
void __builtin_virtual_free(void* const buf) {
  VirtualFree(buf, __builtin_page_size(), MEM_RELEASE);
}

#else  // @todo
#error "Whack currently supports only Windows"
// const int64_t __builtin_page_size() {
//   return 72;
// }

// /// Allocates an executable region of memory
// void* __builtin_virtual_alloc() {
//   return mmap(NULL, __builtin_page_size(), PROT_READ | PROT_WRITE | PROT_EXEC,
//               MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
// }

// /// Frees a region of memory allocated by __builtin_virtual_alloc
// void __builtin_virtual_free(void* const buf) {
//   munmap(buf, __builtin_page_size());
// }

#endif

#ifdef __cplusplus
}
#endif
