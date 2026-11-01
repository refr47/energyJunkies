#!/usr/bin/env python3
"""Host regression tests using production fields, validation and NVS setup code.
Hardware headers are replaced; Setup declarations are extracted unchanged.
"""
from pathlib import Path
import re
import os
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='setup-tests-') as tmp:
    dst = Path(tmp)
    defines = (root / 'include/defines.h').read_text()
    structs = defines[defines.index('typedef struct {\n    char influxHost'):defines.index('} Setup;') + len('} Setup;')]
    constants = '\n'.join(line for line in defines.splitlines() if re.match(r'#define (LEN_WLAN|AMIS_KEY_LEN|MQTT_\w+_LEN|INFLUX_\w+_LEN|EMPTY_VALUE_IN_SETUP)\b', line))
    (dst / 'defines.h').write_text('#pragma once\n#include <cstring>\n#include <cstddef>\n#define INET_ADDRSTRLEN 16\n' + constants + '\n' + structs)
    (dst / 'Arduino.h').write_text('''#pragma once
#include <cstring>
#include <cstddef>
inline size_t strlcpy(char *d,const char*s,size_t n){size_t len=strlen(s);if(n){size_t count=len<n-1?len:n-1;memcpy(d,s,count);d[count]=0;}return len;}
''')
    (dst / 'AsyncJson.h').write_text('#include <ArduinoJson.h>\n')
    templ = (root / 'include/utils/templ.h').read_text().replace('#include "debugConsole.h"', '')
    (dst / 'templ.h').write_text(templ)
    for name in ('setupFields.h', 'setupValidation.h', 'ajaxConst.h'):
        (dst / name).write_text((root / 'include/www' / name).read_text())
    (dst / 'eprom.h').write_text('#include "defines.h"\nbool eprom_storeSetup(Setup&);\nbool eprom_getSetup(Setup&);\nvoid printEprom(Setup&);\nbool eprom_test_write_Eprom(const char*,const char*);\n')
    (dst / 'utils.h').write_text('''#pragma once
#include <cstdio>
#define TAG_EPPROM "eprom"
#define LOG_DEBUG(tag, ...) do { char log[512]; snprintf(log,sizeof(log),__VA_ARGS__); } while(0)
#define LOG_ERROR(tag, ...) LOG_DEBUG(tag,__VA_ARGS__)
''')
    (dst / 'Preferences.h').write_text('''#pragma once
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>
struct Preferences {
 inline static std::vector<unsigned char> bytes;
 inline static unsigned magic=0,schema=0;
 inline static bool failOpen=false,failWrite=false;
 bool begin(const char*,bool){return !failOpen;}
 void end(){}
 size_t putBytes(const char*,const void*p,size_t n){if(failWrite)return 0;bytes.assign((const unsigned char*)p,(const unsigned char*)p+n);return n;}
 size_t getBytes(const char*,void*p,size_t n){if(n!=bytes.size())return 0;memcpy(p,bytes.data(),n);return n;}
 size_t getBytesLength(const char*){return bytes.size();}
 size_t putUInt(const char*k,unsigned v){if(failWrite)return 0;(strcmp(k,"schema")?magic:schema)=v;return sizeof(v);}
 unsigned getUInt(const char*k,unsigned){return strcmp(k,"schema")?magic:schema;}
};
''')
    eprom = (root / 'src/devices/eprom.cpp').read_text().split('/* *************************** SHELLY */')[0]
    (dst / 'eprom.cpp').write_text(eprom)
    cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations', '-I'+tmp,
           '-I'+str(root / '.pio/libdeps/lilygo-t-display-s3/ArduinoJson/src'),
           str(root / 'src/www/setupFields.cpp'), str(root / 'src/www/setupValidation.cpp'),
           str(dst / 'eprom.cpp'), str(root / 'test/setup/test.cpp'), '-o', str(dst / 'test')]
    cmd[1:1] = shlex.split(os.environ.get('SETUP_TEST_CXXFLAGS', ''))
    subprocess.run(cmd, check=True)
    subprocess.run([str(dst / 'test')], check=True)
