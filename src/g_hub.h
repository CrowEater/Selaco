#ifndef __G_HUB_H
#define __G_HUB_H

struct cluster_info_t;
struct wbstartstruct_t;
class FSerializer;
struct FLevelLocals;

int G_GetHubLevelVersion(int levelnum);
void G_SerializeHub (FSerializer &file);
void G_LeavingHub(FLevelLocals *Level, int mode, cluster_info_t * cluster, struct wbstartstruct_t * wbs);

#endif

