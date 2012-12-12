/*
 * cfg.h
 *
 * Copyright 2011-2012 Thomas L Dunnick
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef __CFG__
#define __CFG__

#include "xml.h"

/*
 * common xpaths to values
 */
#define XINSTALL "Phineas.InstallDirectory"
#define XORG "Phineas.Organization"
#define XPARTY "Phineas.PartyId"
#define XRETRY "Phineas.Sender.MaxRetry"
#define XDELAY "Phineas.Sender.DelayRetry"
#define XSOAP "Phineas.SoapTemplate"
#define XACK "Phineas.AckTemplate"
#define XSENDCA "Phineas.Sender.CertificateAuthority"
#define XBASICAUTH "Phineas.Receiver.BasicAuth"
/*
 * common xpath prefixes to configuration
 */
#define XMAP "Phineas.Sender.MapInfo.Map"
#define XROUTE "Phineas.Sender.RouteInfo.Route"
#define XSERVICE "Phineas.Receiver.MapInfo.Map"
#define XQUEUE "Phineas.QueueInfo.Queue"
#define XCONN "Phineas.QueueInfo.Connection"
#define XTYPE "Phineas.QueueInfo.Type"

/*
 * macros for the above...
 */
#define cfg_map_index(x,n) cfg_index((x),XMAP,(n))
#define cfg_route_index(x,n) cfg_index((x),XROUTE,(n))
#define cfg_service_index(x,n) cfg_index((x),XSERVICE,(n))
#define cfg_queue_index(x,n) cfg_index((x),XQUEUE,(n))
#define cfg_conn_index(x,n) cfg_index((x),XCONN,(n))
#define cfg_type_index(x,n) cfg_index((x),XTYPE,(n))
/*
 * macros for getting
 */
#define cfg_installdir(x) xml_get_text((x),XINSTALL)
#define cfg_org(x) xml_get_text((x),XORG)
#define cfg_party(x) xml_get_text((x),XPARTY)
#define cfg_retries(x) xml_get_int((x),XRETRY)
#define cfg_delay(x) xml_get_int((x),XDELAY)
#define cfg_soap(x) xml_get_text((x),XSOAP)
#define cfg_senderca(x) xml_get_text((x),XSENDCA)
#define cfg_timeout(x) 10000

#define cfg_map(x,i,s) xml_getf((x),"%s[%d].%s",XMAP,(i),(s))
#define cfg_route(x,i,s) xml_getf((x),"%s[%d].%s",XROUTE,(i),(s))
#define cfg_service(x,i,s) xml_getf((x),"%s[%d].%s",XSERVICE,(i),(s))
#define cfg_queue(x,i,s) xml_getf((x),"%s[%d].%s",XQUEUE,(i),(s))
#define cfg_conn(x,i,s) xml_getf((x),"%s[%d].%s",XCONN,(i),(s))
#define cfg_type(x,i,s) xml_getf((x),"%s[%d].%s",XTYPE,(i),(s))

extern char ConfigName[];
extern XML *Config;

/* used for configuration encryption				*/
extern char *ConfigCert;
extern char *ConfigPass;
extern int ConfigAlgorithm;

/*
 * remove formatted config
 */
void cfg_clean (char *path);
/*
 * free up configuration and remove formatted config
 */
void cfg_free ();
/*
 * load a configuration, decrypting it if credentials found
 */
XML *cfg_load (char *path);
/*
 * save a configuration, encrypting it if credentials found
 */
int cfg_save (XML *xml, char *path);
/*
 * create a censored configuration for display purposes, and return
 * it's name.
 */
char *cfg_format ();
/*
 * find the index for a repeated configuration item
 */
int cfg_index (XML *xml, char *path, char *name);

#endif /* __CFG__ */
