#ifndef WEB_CONFIG_H_
#define WEB_CONFIG_H_



int build_dynamic_webconfig_js(struct aqualinkdata *aqdata, char* buffer, int size);
int save_web_config_json(const char* inBuf, int inSize, char* outBuf, int outSize, struct aqualinkdata *aqdata);


#endif