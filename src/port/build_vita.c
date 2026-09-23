#ifdef __vita__

#include <stdint.h>

#ifndef PAPERBOAT_VITA_GIT_BRANCH
#define PAPERBOAT_VITA_GIT_BRANCH "unknown"
#endif

#ifndef PAPERBOAT_VITA_GIT_COMMIT
#define PAPERBOAT_VITA_GIT_COMMIT "unknown"
#endif

const char gBuildVersion[] = "Mulberry Bravo (1.0.1)";
const uint16_t gBuildVersionMajor = 1;
const uint16_t gBuildVersionMinor = 0;
const uint16_t gBuildVersionPatch = 1;

const char gGitBranch[] = PAPERBOAT_VITA_GIT_BRANCH;
const char gGitCommitHash[] = PAPERBOAT_VITA_GIT_COMMIT;
const char gGitCommitTag[] = "";

const char gBuildTeam[] = "Caladius";
const char gBuildDate[] = __DATE__ " " __TIME__;
const char gBuildMakeOption[] = "vita";

#endif
