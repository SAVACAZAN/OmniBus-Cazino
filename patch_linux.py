#!/usr/bin/env python3
"""Patch CMakeLists.txt for Linux cross-platform build."""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "CMakeLists.txt"

with open(path, "r") as f:
    lines = f.readlines()

new = []
i = 0
while i < len(lines):
    line = lines[i]

    if "# OpenSSL" in line and "manual imported" in line:
        new.append("# OpenSSL -- cross-platform\n")
        new.append("if(WIN32)\n")
        i += 1
        while i < len(lines):
            new.append("  " + lines[i])
            if "target_link_libraries(OpenSSL::SSL" in lines[i]:
                i += 1
                break
            i += 1
        new.append("else()\n")
        new.append("  find_package(OpenSSL REQUIRED)\n")
        new.append("endif()\n")
        continue

    if '${OPENSSL_BASE}/include' in line and 'target_include' not in lines[max(0,i-1)].lower():
        new.append(line)
        i += 1
        continue

    new.append(line)
    i += 1

# Remove the hardcoded Windows include from target_include_directories
final = "".join(new)
final = final.replace('"${OPENSSL_BASE}/include"', "")

with open(path, "w") as f:
    f.write(final)

print("PATCHED for Linux!")
