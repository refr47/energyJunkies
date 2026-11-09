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
    (dst / 'defines.h').write_text('#pragma once\n#include <cstring>\n#include <cstddef>\n#include \"energyPolling.h\"\n#include \"gridDemandAspect.h\"\n#include \"heatingPlan.h\"\n#define INET_ADDRSTRLEN 16\n' + constants + '\n' + structs)
    (dst / 'gridDemandAspect.h').write_text((root / 'include/gridDemandAspect.h').read_text())
    (dst / 'heatingPlan.h').write_text((root / 'include/heatingPlan.h').read_text())
    (dst / 'energyPolling.h').write_text((root / 'include/energyPolling.h').read_text())
    (dst / 'setupLegacy.h').write_text((root / 'include/devices/setupLegacy.h').read_text())
    (dst / 'Arduino.h').write_text('''#pragma once
#include <cstring>
#include <cstddef>
#include <string>
using String = std::string;
inline size_t strlcpy(char *d,const char*s,size_t n){size_t len=strlen(s);if(n){size_t count=len<n-1?len:n-1;memcpy(d,s,count);d[count]=0;}return len;}
''')
    (dst / 'AsyncJson.h').write_text('#include <ArduinoJson.h>\n')
    templ = (root / 'include/utils/templ.h').read_text().replace('#include "debugConsole.h"', '')
    (dst / 'templ.h').write_text(templ)
    for name in ('setupFields.h', 'setupValidation.h', 'ajaxConst.h'):
        (dst / name).write_text((root / 'include/www' / name).read_text())
    (dst / 'eprom.h').write_text('#include "defines.h"\nbool eprom_storeSetup(Setup&,uint32_t* = nullptr);\nbool eprom_getSetup(Setup&,uint32_t* = nullptr);\nuint32_t eprom_setupRevision();\nvoid printEprom(Setup&);\nbool eprom_test_write_Eprom(const char*,const char*);\n')
    (dst / 'utils.h').write_text('''#pragma once
#include <cstdio>
#define TAG_EPPROM "eprom"
#define LOG_DEBUG(tag, ...) do { char log[512]; snprintf(log,sizeof(log),__VA_ARGS__); } while(0)
#define LOG_ERROR(tag, ...) LOG_DEBUG(tag,__VA_ARGS__)
''')
    (dst / 'Preferences.h').write_text('''#pragma once
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <cstdint>
struct Preferences {
 inline static std::map<std::string,std::vector<unsigned char>> blobs;
 inline static unsigned magic=0,schema=0;
 inline static bool failOpen=false,failWrite=false;
 bool begin(const char*,bool){return !failOpen;}
 void end(){}
 size_t putBytes(const char*k,const void*p,size_t n){if(failWrite)return 0;blobs[k].assign((const unsigned char*)p,(const unsigned char*)p+n);return n;}
 size_t getBytes(const char*k,void*p,size_t n){auto &b=blobs[k];if(n!=b.size())return 0;memcpy(p,b.data(),n);return n;}
 size_t getBytesLength(const char*k){return blobs[k].size();}
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

    (dst / 'hotUpdate.h').write_text((root / 'include/utils/hotUpdate.h').read_text())
    (dst / 'app_state.h').write_text('''#pragma once
#include "defines.h"
#include <cassert>
struct TestEnergy { bool valid=false; bool saving=false; };
struct TestData { Setup setupData; TestEnergy energyState; };
struct TestApp { TestData webSockData; };
inline TestApp g_app{};
inline bool failConfig=false,failPid=false,dataLocked=false,configLocked=false,pidLocked=false;
inline bool appLockData(unsigned){assert(!dataLocked);dataLocked=true;return true;}
inline bool appLockConfig(unsigned){assert(dataLocked && !configLocked);if(failConfig)return false;configLocked=true;return true;}
inline bool appLockPidOut(unsigned){assert(configLocked && !pidLocked);if(failPid)return false;pidLocked=true;return true;}
inline void appUnlockPidOut(){assert(pidLocked);pidLocked=false;}
inline void appUnlockConfig(){assert(configLocked && !pidLocked);configLocked=false;}
inline void appUnlockData(){assert(dataLocked && !configLocked);dataLocked=false;}
''')
    cmd[cmd.index(str(root / 'test/setup/test.cpp'))] = str(root / 'test/setup/hot_update.cpp')
    cmd.insert(-2, str(root / 'src/utils/hotUpdate.cpp'))
    subprocess.run(cmd, check=True)
    subprocess.run([str(dst / 'test')], check=True)
