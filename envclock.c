/* envClock
gcc -Os -N -o envClock envclock.c -lauto
strip envClock
strip -R.comment envClock
*/

#define __USE_INLINE__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/commodities.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/locale.h>
#include <proto/wb.h>

#include <workbench/startup.h>

#include "envClock_rev.h"

#ifdef __amigaos4__
#define CurrentDir SetCurrentDir
#else
#define TimeRequest timerequest
#endif

const char __attribute__((used)) *version = VERSTAG;

/* Global config */
int poll = 60; //seconds
char *envvar = NULL;
char *format = NULL;

/* Global vars */
struct MsgPort *msgport;
struct TimeRequest *tioreq;
CxObj *broker;
struct MsgPort *broker_mp;
struct Hook *cbhook;
char *p;

struct NewBroker newbroker = {
	NB_VERSION,
	"envClock",
	VERS,
	"Titlebar clock",
	NBU_UNIQUE | NBU_NOTIFY, /* unique*/
	0, /* flags */
	0, /*pri*/
	0, /*msgport*/
	0
};

#ifdef __amigaos4__
static void formatdate_cb(struct Hook *hook, struct Locale *loc, TEXT ch)
#else
static void __saveds formatdate_cb(struct Hook *hook __asm("a0"), struct Locale *loc __asm("a2"), TEXT cb __asm("a1"))
#endif
{
	*p = ch;
	p++;
}

BOOL updateenv(void)
{
	struct DateStamp ds;	
	char date[100];

	p = date;
	DateStamp(&ds);
	FormatDate(NULL, format, &ds, cbhook);
	*p = '\0';

	SetVar(envvar, date, -1, GVF_GLOBAL_ONLY);

	return TRUE;
}

void killpoll()
{
	if(CheckIO((struct IORequest *)tioreq)==0) {
    	AbortIO((struct IORequest *)tioreq);
    	WaitIO((struct IORequest *)tioreq);
	}
}

void startpoll(int time)
{
#ifdef __amigaos4__
	tioreq->Request.io_Command=TR_ADDREQUEST;
	tioreq->Time.Seconds = time;
	tioreq->Time.Microseconds = 0L;
#else
	tioreq->tr_node.io_Command=TR_ADDREQUEST; 
	tioreq->tr_time.tv_secs = time;
	tioreq->tr_time.tv_micro = 0L;
#endif
	SendIO((struct IORequest *)tioreq);
}

/* Sync and start polling, returns FALSE if no new poll set up */
BOOL env_poll()
{
	if (updateenv()) {
		if(poll > 0) {
			startpoll(poll);
		} else {
			return FALSE;
		}
	}

	return TRUE;
}

void main_loop()
{
	struct Message *msg;
	ULONG sigrcvd, msgid, msgtype;
	ULONG pollsig = 1L << msgport->mp_SigBit;
	ULONG cxsigflag = 1L << broker_mp->mp_SigBit;
	BOOL running = TRUE;

	while(running) {
		sigrcvd = Wait(pollsig | cxsigflag | SIGBREAKF_CTRL_C);

		if(sigrcvd & pollsig) {
			while(msg = GetMsg(msgport)) {
				// ReplyMsg(msg);
				running = env_poll();
			}
		}

		if(sigrcvd & cxsigflag) {
			while(msg = GetMsg(broker_mp)) {
				msgid = CxMsgID((CxMsg *)msg);
				msgtype = CxMsgType((CxMsg *)msg);
				ReplyMsg(msg);

				switch(msgtype) {
					case CXM_COMMAND:
						switch(msgid) {
							case CXCMD_DISABLE:
								killpoll();
								ActivateCxObj(broker,0L);
							break;
							case CXCMD_ENABLE:
								running = env_poll();
								ActivateCxObj(broker,1L);
							break;
							case CXCMD_KILL:
							case CXCMD_UNIQUE:
								running = FALSE;
							break;
						}
					break;
					default:
					break;
				}
			}
		}

		if(sigrcvd & SIGBREAKF_CTRL_C)
		{
			running = FALSE;
		}
	}
}

void gettooltypes(struct WBArg *wbarg)
{
	struct DiskObject *dobj;
	STRPTR *toolarray;
	char *s;

	if((*wbarg->wa_Name) && (dobj=GetDiskObject(wbarg->wa_Name))) {
		toolarray = (STRPTR *)dobj->do_ToolTypes;
		if(s = (char *)FindToolType(toolarray,"UPDATETIME")) poll = atoi(s);
		if(s = (char *)FindToolType(toolarray,"ENVVAR")) {
			envvar = strdup(s);
		} else {
			envvar = strdup("time");
		}
		if(s = (char *)FindToolType(toolarray,"FORMAT")) {
			format = strdup(s);
		} else {
			format = strdup("%d %b %Y %H:%M");
		}

		FreeDiskObject(dobj);
	}
}


BOOL startcx(void)
{
	if(broker_mp = CreateMsgPort()) {
		newbroker.nb_Port = broker_mp;
		if(broker = CxBroker(&newbroker,NULL)) {
			ActivateCxObj(broker,1L);
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

void wbcleanup(void)
{
	struct Message *msg;

	/* string cleanup */
	if(envvar) free(envvar);
	if(format) free(format);

#ifdef __amigaos4__
	FreeSysObject(ASOT_HOOK, cbhook);
#else
	FreeVec(cbhook);
#endif

	if(broker) DeleteCxObj(broker);
	if(broker_mp)
	{
		while(msg = GetMsg(broker_mp))
			ReplyMsg(msg);
		DeleteMsgPort(broker_mp);
	}

	if(tioreq)
	{
		while(msg = GetMsg(msgport)) {
        	// ReplyMsg(msg);
		}
		if(CheckIO((struct IORequest *)tioreq)==0)
		{
		    AbortIO((struct IORequest *)tioreq);
		    WaitIO((struct IORequest *)tioreq);
		}

		CloseDevice((struct IORequest *) tioreq);
		DeleteIORequest((struct IORequest *)tioreq);
		DeleteMsgPort(msgport);
	}
}

int main(int argc, char **argv)
{
	struct WBStartup *WBenchMsg;
	struct WBArg *wbarg;
	char i;
	LONG olddir =-1;
	int err;
	int rc = 0;

	if(argc != 0) {
		// cli startup
		printf("%s", VSTRING);
		printf("http://www.unsatisfactorysoftware.co.uk\n\n");
		printf("This program must be run from Workbench.\n");
		rc = 10;
	} else {
		WBenchMsg = (struct WBStartup *)argv;
		for(i=0,wbarg=WBenchMsg->sm_ArgList;i<WBenchMsg->sm_NumArgs;i++,wbarg++) {
			olddir =-1;
			if((wbarg->wa_Lock)&&(*wbarg->wa_Name))
				olddir = CurrentDir(wbarg->wa_Lock);

			gettooltypes(wbarg);

			if(olddir !=-1) CurrentDir(olddir);
		}

		if(startcx()) {
#ifdef __amigaos4__
			cbhook = AllocSysObjectTags(ASOT_HOOK,
						ASOHOOK_Entry, formatdate_cb,
						TAG_DONE);
#else
			if(cbhook = AllocVec(sizeof(struct Hook), MEMF_CLEAR)) {
				cbhook->h_Entry = formatdate_cb;
				cbhook->h_SubEntry = NULL;
				cbhook->h_Data = NULL;
			}
#endif
			msgport = CreateMsgPort();
			tioreq= (struct TimeRequest *)CreateIORequest(msgport,sizeof(struct MsgPort));
			OpenDevice("timer.device",UNIT_VBLANK,(struct IORequest *)tioreq,0);

			if(env_poll()) {
				main_loop();
			}

			wbcleanup();
		}
	}

	return rc;
}
