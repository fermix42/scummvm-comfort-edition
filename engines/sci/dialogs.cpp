/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "engines/advancedDetector.h"
#include "base/plugins.h"
#include "common/file.h"
#include "common/hashmap.h"
#include "common/ptr.h"
#include "common/translation.h"

#include "sci/detection.h"
#include "sci/dialogs.h"
#include "sci/galaxy_data.h"
#include "audio/decoders/wave.h"
#include "audio/mixer.h"
#include "audio/audiostream.h"
#include "common/fs.h"
#include "sci/sci.h"
#include "sci/engine/tts.h"

#include "sci/detection_options.h"
#include "common/system.h"
#include "sci/event.h"
#include "sci/engine/state.h"
#include "sci/engine/script.h"
#include "sci/engine/selector.h"
#include "sci/engine/kernel.h"
#include "sci/engine/seg_manager.h"
#include "sci/graphics/ports.h"
#include "sci/graphics/animate.h"
#include "sci/graphics/paint16.h"
#include "sci/graphics/screen.h"
#include "sci/graphics/cache.h"
#include "sci/graphics/scifont.h"
#include "sci/graphics/cursor.h"
#include "sci/graphics/palette16.h"
#include "sci/sound/soundcmd.h"

namespace Sci {

// Draw into SCI's own 320x200 pause window, so the audio page shares the
// original game's palette, font, and Save/Restore/Play buttons.
static void saveSQ5Audio() {
	// CE owns the active target's mixer settings; the external launcher's
	// audio-settings.ini and SoundFont selection do not apply here.
	g_sci->updateSoundMixerVolumes();
	ConfMan.flushToDisk();
}

void sq5AudioMenu(SciEvent &event, int argc, reg_t *argv) {
	if (!g_sci->isSQ5TrashTalkin() || !ConfMan.hasKey("sq5_voice_path")) return;
	static Common::Array<byte> background;
	static bool visible = false, forwardingButton = false;
	static int drag = -1, menuPort = -1, detail = 3, speed = 8;
	EngineState *s = g_sci->getEngineState();
	bool inMenu = false;
	for (const auto &frame : s->_executionStack)
		if (Common::String(s->_segMan->getObjectName(frame.objp)) == "gameControlCode") { inMenu = true; break; }
	if (!inMenu) { background.clear(); visible=false; forwardingButton=false; drag=-1; menuPort=-1; return; }
	Port *port = g_sci->_gfxPorts->getPort();
	if (menuPort >= 0 && menuPort != port->id) return;
	const Common::Rect area(18,42,303,190);
	Common::Rect local=area; local.translate(-port->left,-port->top);
	auto original = [&]() {
		if (visible) { g_sci->_gfxScreen->bitsRestore(&background[0]); g_sci->_gfxPaint16->bitsShow(local); }
		visible=false; drag=-1;
	};
	// Original buttons track both press and release using subsequent GetEvent
	// calls. Keep their entire gesture in the original coordinate system.
	if (forwardingButton) {
		event.mousePos.x += 52;
		if (event.type == kSciEventMouseRelease) forwardingButton = false;
		return;
	}
	reg_t detailObj=s->_segMan->findObjectByName("detailSlider");
	reg_t speedObj=s->_segMan->findObjectByName("speedSlider");
	auto value = [&](reg_t obj, int newValue) {
		reg_t oldAcc=s->r_acc, arg=make_reg(0,newValue);
		invokeSelector(s,obj,SELECTOR(doit),argc,argv,newValue<0?0:1,&arg);
		int result=s->r_acc.toUint16(); s->r_acc=oldAcc; return result;
	};
	if (!visible) {
		background.resize(g_sci->_gfxScreen->bitsGetDataSize(area,1));
		g_sci->_gfxScreen->bitsSave(area,1,&background[0]);
		menuPort=port->id; visible=true;
		detail=value(detailObj,-1); speed=value(speedObj,-1);
	}
	const bool press=event.type==kSciEventMousePress;
	// Restore the original window before forwarding any original button action.
	// The script then owns its usual dialogs and restores the gameplay screen.
	if ((press && Common::Rect(28,52,80,170).contains(event.mousePos)) || event.type==kSciEventKeyDown || event.type==kSciEventQuit) {
		original();
		if (press) { event.mousePos.x+=52; forwardingButton=true; }
		return;
	}
	const int centers[]={105,148,191,234,277};
	const char *keys[]={"music_volume","speech_volume","sfx_volume"};
	bool speedEnabled=!(readSelectorValue(s->_segMan,speedObj,SELECTOR(signal)) & 4);
	if (press) {
		for (int i=0;i<5;++i)
			if (Common::Rect(centers[i]-13,76,centers[i]+14,141).contains(event.mousePos) && (i!=4 || speedEnabled)) drag=i;

	}
	if (drag>=0) {
		int amount=CLIP<int>(136-event.mousePos.y,0,54);
		if (drag==0) { int n=1+(amount*2+27)/54; if(n!=detail) detail=value(detailObj,n); }
		else if (drag==4) { int n=15-(amount*15+27)/54; if(n!=speed) speed=value(speedObj,n); }
		else {
			int n=(amount*255+27)/54;
			if (n!=ConfMan.getInt(keys[drag-1])) { ConfMan.setInt(keys[drag-1],n); g_sci->updateSoundMixerVolumes(); }
		}
		if(event.type==kSciEventMouseRelease) { saveSQ5Audio(); drag=-1; }
	}
	if(event.type==kSciEventMousePress || event.type==kSciEventMouseRelease) event.type=kSciEventNone;
	auto fill=[&](int x,int y,int w,int h,byte c) {
		g_sci->_gfxPaint16->fillRect(Common::Rect(x-port->left,y-port->top,x+w-port->left,y+h-port->top),1,c);
	};
	auto text=[&](int x,int y,const char *label,int font=0) {
		reg_t args[]={make_reg(0,100),make_reg(0,x-port->left),make_reg(0,y-port->top),make_reg(0,102),make_reg(0,0),make_reg(0,105),make_reg(0,font)};
		g_sci->_gfxPaint16->kernelDisplay(label,0,7,args);
	};
	auto centered=[&](int x,int y,const char *label,int font=4) {
		GfxFont *f=g_sci->_gfxCache->getFont(font); int width=0;
		for(const char *c=label;*c;++c) width+=f->getCharWidth((byte)*c);
		text(x-width/2,y,label,font);
	};
	auto cel=[&](int loop,int celNo,int x,int y) {
		g_sci->_gfxPaint16->drawCelAndShow(995,loop,celNo,x-port->left,y-port->top,255,0);
	};
	// Use the original window bevel colors and original button/slider cels.
	fill(20,44,282,144,0); fill(18,42,281,144,7); fill(19,44,280,142,6);
	fill(21,45,278,141,3); fill(21,45,276,139,5);
	int loops[]={2,3,4,5,8};
	for(int i=0;i<5;++i) { fill(26,50+20*i,54,19,0); fill(27,51+20*i,52,17,7); cel(loops[i],0,28,52+20*i); }
	fill(26,150,54,19,0); fill(27,151,52,17,7); cel(6,0,28,152); cel(7,0,54,152);
	centered(192,50,"GAME PAUSED",0);
	const char *labels[]={"DETAIL","MUSIC","VOICE","EFFECTS","SPEED"};
	for(int i=0;i<5;++i) {
		centered(centers[i],67,labels[i]);
		cel(0,1,centers[i]-10,78);
		int amount=i==0?(detail-1)*27:i==4?(15-speed)*54/15:ConfMan.getInt(keys[i-1])*54/255;
		cel(0,0,centers[i]-10,136-amount);
	}
	centered(193,154,"Music device: CE Game Options",4);
	g_sci->_gfxPaint16->bitsShow(local);
}


// SQ5's chart replaces only the bridge coordinate pad (script 203 in room
// 201). Its other use, the colony computer password, remains untouched.
// No new save fields: discovery comes from the game's original saved flags.
// Announce only at an idle bridge input poll, using the native two-talker
// dialogue lifecycle. Match script 218's actual cloak readiness conditions.
void sq5CloakAnnouncement(SciEvent &event, int argc, reg_t *argv) {
	if(!g_sci->isSQ5TrashTalkin() || !ConfMan.hasKey("sq5_voice_path") || event.type!=kSciEventNone) return;
	EngineState *s=g_sci->getEngineState();
	if(s->sq5CloakAnnounced>=2 || s->abortScriptProcessing!=kAbortNone) return;
	// say() queues talkers. Restore input only once Messager has disposed
	// the final reply, not when say() returns. Value 1 also repairs saves
	// written by the original announcement implementation.
	if(s->sq5CloakAnnounced==1) {
		SegManager *seg=s->_segMan;
		const reg_t messager=s->variables[VAR_GLOBAL][91];
		if(!s->variables[VAR_GLOBAL][84].isNull() ||
		   (seg->getObject(messager) && readSelectorValue(seg,messager,g_sci->getKernel()->findSelector("oldIconBarState")))) return;
		const Selector scriptSel=g_sci->getKernel()->findSelector("script");
		for(int index=0;index<=2;++index) {
			const reg_t obj=s->variables[VAR_GLOBAL][index];
			if(seg->getObject(obj) && !readSelector(seg,obj,scriptSel).isNull()) return;
		}
		s->sq5CloakAnnounced=2;
		const reg_t acc=s->r_acc;
		invokeSelector(s,s->variables[VAR_GLOBAL][1],g_sci->getKernel()->findSelector("handsOn"),argc,argv,0,nullptr);
		s->r_acc=acc;
		return;
	}
	auto flag=[&](int n) {return (s->variables[VAR_GLOBAL][183+n/16].toUint16() & (0x8000>>(n%16)))!=0;};
	// Original saves retain flag 194 after successfully cloaking during the
	// Goliath encounter, even after decloaking. Flag 39 covers current use.
	if(flag(39) || flag(194)) {
		s->sq5CloakAnnounced=2;
		return;
	}
	if(s->currentRoomNumber()!=201) return;
	if(!flag(89) || !flag(75) || !flag(94) || s->variables[VAR_GLOBAL][142].toUint16()!=2 || flag(32) || flag(37)) return;
	SegManager *seg=s->_segMan;
	const reg_t user=seg->findObjectByName("User");
	if(user.isNull() || !readSelectorValue(seg,user,g_sci->getKernel()->findSelector("input")) ||
	   !readSelectorValue(seg,user,g_sci->getKernel()->findSelector("controls"))) return;
	const Selector scriptSel=g_sci->getKernel()->findSelector("script");
	for(int index=0;index<=2;++index) {
		const reg_t obj=s->variables[VAR_GLOBAL][index];
		if(seg->getObject(obj) && !readSelector(seg,obj,scriptSel).isNull()) return;
	}
	// Mark before entering say: its event polling must not trigger this again.
	s->sq5CloakAnnounced=1;
	const reg_t acc=s->r_acc;
	const reg_t params[]={make_reg(0,200),NULL_REG,NULL_REG,NULL_REG,NULL_REG,make_reg(0,201)};
	invokeSelector(s,s->variables[VAR_GLOBAL][1],g_sci->getKernel()->findSelector("handsOff"),argc,argv,0,nullptr);
	invokeSelector(s,s->variables[VAR_GLOBAL][91],g_sci->getKernel()->findSelector("say"),argc,argv,6,params);
	s->r_acc=acc;
}

// Optional duct shortcut. Runs only at an idle gameplay poll and never changes
// User input/controls or queues a native talker: all overlay state is restored
// before requesting the original room transition.
void sq5MazeShortcut(SciEvent &event, int argc, reg_t *argv) {
	if (!g_sci->isSQ5TrashTalkin() || !ConfMan.hasKey("sq5_voice_path")) return;
	EngineState *s=g_sci->getEngineState();
	if(s->sq5MazeFadePending && s->sq5MazeChoice==2 && s->currentRoomNumber()==1020) {
		SegManager *seg=s->_segMan;
		bool ready=event.type==kSciEventNone && s->variables[VAR_GLOBAL][84].isNull();
		for(int i=0;i<=2 && ready;++i) {
			reg_t obj=s->variables[VAR_GLOBAL][i];
			if(!seg->getObject(obj))continue;
			reg_t script=readSelector(seg,obj,g_sci->getKernel()->findSelector("script"));
			if(script.isNull())continue;
			Common::String name=seg->getObjectName(script);
			ready=i==2 && (name=="sEnterWest" || name=="sEnterEast" || name=="sEnterNorth" || name=="sEnterSouth") &&
				readSelectorValue(seg,script,g_sci->getKernel()->findSelector("state"))==1;
		}
		// Reveal on the next completed Animate, never from its restored
		// background buffer between frames (which contains no moving actors).
		if(ready)s->sq5MazeFadeReady=true;
		return;
	}
	if(s->currentRoomNumber()!=1020 || s->sq5MazeChoice || s->abortScriptProcessing!=kAbortNone) return;
	const uint16 place=s->variables[VAR_GLOBAL][133].toUint16()*36+s->variables[VAR_GLOBAL][135].toUint16();
	if(s->sq5MazeOrigin==0xffff) {s->sq5MazeOrigin=place;return;}
	if(place==s->sq5MazeOrigin || event.type!=kSciEventNone || !s->variables[VAR_GLOBAL][84].isNull()) return;
	// If the player already found the switch, don't offer a pointless rewind.
	if(s->variables[VAR_GLOBAL][183+247/16].toUint16() & (0x8000>>(247%16))) {s->sq5MazeChoice=1;return;}
	SegManager *seg=s->_segMan;
	const Selector scriptSel=g_sci->getKernel()->findSelector("script");
	for(int i=0;i<=2;++i) {
		reg_t obj=s->variables[VAR_GLOBAL][i];
		if(seg->getObject(obj)) {
			reg_t script=readSelector(seg,obj,scriptSel);
			if(!script.isNull()) {
				Common::String name=seg->getObjectName(script);
				// Entry scripts deliberately stay attached in state 1 while
				// waiting for the player's next direction; this IS maze idle.
				bool waiting=i==2 && (name=="sEnterWest" || name=="sEnterEast" || name=="sEnterNorth" || name=="sEnterSouth") &&
					readSelectorValue(seg,script,g_sci->getKernel()->findSelector("state"))==1;
				if(!waiting)return;
			}
		}
	}
	// Do not interrupt a menu, another modal dialog, or the crawl animation.
	for(const auto &frame:s->_executionStack)
		if(Common::String(seg->getObjectName(frame.objp))=="gameControlCode") return;
	reg_t user=seg->findObjectByName("User");
	if(user.isNull() || !readSelectorValue(seg,user,g_sci->getKernel()->findSelector("input"))) return;
	const reg_t oldAcc=s->r_acc;
	Port *port=g_sci->_gfxPorts->getPort();
	const Common::Rect full(0,0,320,200);
	Common::Rect local=full;local.translate(-port->left,-port->top);
	g_sci->_gfxScreen->syncMazeStatusBar();
	Common::Array<byte> background;
	background.resize(g_sci->_gfxScreen->bitsGetDataSize(full,1));
	g_sci->_gfxScreen->bitsSave(full,1,&background[0]);
	byte palette[768];g_sci->_gfxScreen->grabPalette(palette,0,256);
	Audio::SoundHandle voice,metal;
	auto play=[&](const char *id,Audio::SoundHandle &handle,Audio::Mixer::SoundType type,int volume=255) {
		g_system->getMixer()->stopHandle(handle);
		Common::File *f=new Common::File();
		if(!f->open(Common::FSNode(ConfMan.getPath("sq5_voice_path").join(Common::String(id)+".wav")))) {delete f;return uint32(0);}
		Audio::SeekableAudioStream *stream=Audio::makeWAVStream(f,DisposeAfterUse::YES);
		if(!stream) return uint32(0);
		uint32 length=stream->getLength().msecs();
		g_system->getMixer()->playStream(type,&handle,stream,-1,volume);
		return length;
	};
	auto fill=[&](int x,int y,int w,int h,int c) {
		g_sci->_gfxPaint16->fillRect(Common::Rect(x-port->left,y-port->top,x+w-port->left,y+h-port->top),1,c);
	};
	const int mazeFont=readSelectorValue(seg,s->variables[VAR_GLOBAL][89],g_sci->getKernel()->findSelector("font"));
	auto text=[&](int x,int y,const char *label,int color=0) {
		reg_t a[]={make_reg(0,100),make_reg(0,x-port->left),make_reg(0,y-port->top),make_reg(0,102),make_reg(0,color),make_reg(0,105),make_reg(0,mazeFont)};
		Common::String upper(label);upper.toUppercase();
		g_sci->_gfxPaint16->kernelDisplay(upper.c_str(),0,7,a);
	};
	bool showMazeActors=true;
	Common::Rect panelRect;
	auto show=[&]() {
		if(showMazeActors) {
			// The visual buffer excludes transient cels between Animate calls.
			// Recompose the unchanged scene, then put the dialog over it.
			Port *saved=g_sci->_gfxPorts->getPort();
			g_sci->_gfxPorts->setPort((Port *)g_sci->_gfxPorts->_picWind);
			Common::Rect rect=full;rect.translate(-g_sci->_gfxPorts->_picWind->left,-g_sci->_gfxPorts->_picWind->top);
			g_sci->_gfxAnimate->reAnimate(rect);
			g_sci->_gfxPorts->setPort(saved);
			if(!panelRect.isEmpty()) {
				// Window-port clipping excludes the bevel and shadow. Copy
				// the complete absolute window rectangle, including its frame.
				g_sci->_gfxScreen->copyRectToScreen(panelRect);
			}
		} else g_sci->_gfxPaint16->bitsShow(local);
		g_system->updateScreen();
	};
	bool quit=false;
	auto poll=[&]() {
		SciEvent input=g_sci->getEventManager()->getSciEvent(kSciEventAny);
		if(input.type==kSciEventQuit || g_sci->shouldQuit()) {quit=true;event=input;}
		return input;
	};
	auto wait=[&](uint32 ms) {
		uint32 start=g_system->getMillis();
		while(!quit && g_system->getMillis()-start<ms) {poll();g_system->delayMillis(16);}
	};
	reg_t nativeWindow=NULL_REG;
	Port *originalPort=port;
	auto closePanel=[&]() {
		if(!nativeWindow.isNull()) {
			invokeSelector(s,nativeWindow,g_sci->getKernel()->findSelector("dispose"),argc,argv,0,nullptr);
			nativeWindow=NULL_REG;panelRect=Common::Rect();
			g_sci->_gfxPorts->setPort(originalPort);port=originalPort;
		}
	};
	auto panel=[&](int left,int top,int right,int bottom) {
		closePanel();
		reg_t cls=s->variables[VAR_GLOBAL][38]; // Actual sq5Win template used by Print.
		invokeSelector(s,cls,g_sci->getKernel()->findSelector("new"),argc,argv,0,nullptr);
		nativeWindow=s->r_acc;
		auto prop=[&](const char *name,int value){writeSelectorValue(seg,nativeWindow,g_sci->getKernel()->findSelector(name),value);};
		prop("top",top-originalPort->top);prop("left",left-originalPort->left);prop("bottom",bottom-originalPort->top);prop("right",right-originalPort->left);
		prop("back",5);prop("priority",-1);
		invokeSelector(s,nativeWindow,g_sci->getKernel()->findSelector("open"),argc,argv,0,nullptr);
		port=g_sci->_gfxPorts->getPort();
		panelRect=Common::Rect(left-4,top-4,right+6,bottom+6);
	};
	// Use the same paragraph rhythm as native conversation choices: an
	// inset on every side, font-measured wrapping, and one blank line between
	// the prompt and each response. Hit areas follow the wrapped paragraphs.
	GfxFont *font=g_sci->_gfxCache->getFont(mazeFont);
	const int lineHeight=font->getHeight()+1;
	const int inset=9, boxWidth=200, contentWidth=boxWidth-2*inset;
	auto wrap=[&](const char *label) {
		Common::Array<Common::String> lines;
		Common::String upper(label),line,word;upper.toUppercase();
		int width=0;
		for(uint i=0;i<=upper.size();++i) {
			if(i<upper.size() && upper[i]!=' ') {word+=upper[i];continue;}
			int wordWidth=0;for(uint j=0;j<word.size();++j)wordWidth+=font->getCharWidth((byte)word[j]);
			const int space=line.empty()?0:font->getCharWidth(' ');
			if(!line.empty() && width+space+wordWidth>contentWidth) {lines.push_back(line);line.clear();width=0;}
			if(!line.empty()){line+=' ';width+=font->getCharWidth(' ');}
			line+=word;width+=wordWidth;word.clear();
		}
		if(!line.empty())lines.push_back(line);
		return lines;
	};
	auto paragraph=[&](int x,int y,const Common::Array<Common::String> &lines,int color) {
		for(uint i=0;i<lines.size();++i)text(x,y+i*lineHeight,lines[i].c_str(),color);
	};
	const auto prompt=wrap("Warning: The following is a gratuitous maze sequence. We'd like to offer to spare you the indignity. Of course you can stay down here and wander around if you want...");
	const auto first=wrap("Spare me, I've suffered enough. (SKIP)");
	const auto second=wrap("Don't tell me how to live my life.");
	int left=(320-boxWidth)/2,x=left+inset;
	const int promptHeight=2*inset+prompt.size()*lineHeight;
	const int promptTop=(200-promptHeight)/2;
	panel(left,promptTop,left+boxWidth,promptTop+promptHeight);
	paragraph(x,promptTop+inset,prompt,0);show();
	const uint32 promptDuration=play("1020-200-0-0-1",voice,Audio::Mixer::kSpeechSoundType);
	const uint32 promptStart=g_system->getMillis();
	while(!quit && g_system->getMillis()-promptStart<(promptDuration?promptDuration+250:12000)) {
		SciEvent input=poll();
		if(input.type==kSciEventMousePress || input.type==kSciEventKeyDown)break;
		g_system->delayMillis(16);
	}
	g_system->getMixer()->stopHandle(voice);
	int choiceTextWidth=0;
	for(const auto &lines : {first,second}) {
		for(const auto &line : lines) {
			int width=0;for(uint i=0;i<line.size();++i)width+=font->getCharWidth((byte)line[i]);
			choiceTextWidth=MAX(choiceTextWidth,width);
		}
	}
	const int choiceWidth=choiceTextWidth+2*inset;
	left=(320-choiceWidth)/2;x=left+inset;
	const int boxHeight=2*inset+(first.size()+second.size()+1)*lineHeight;
	const int top=(200-boxHeight)/2,firstY=top+inset;
	const int secondY=firstY+(first.size()+1)*lineHeight;
	const Common::Rect firstHit(x,firstY-2,x+choiceTextWidth,firstY+first.size()*lineHeight+2);
	const Common::Rect secondHit(x,secondY-2,x+choiceTextWidth,secondY+second.size()*lineHeight+2);
	if(!quit) {
		panel(left,top,left+choiceWidth,top+boxHeight);
		paragraph(x,firstY,first,0);
		paragraph(x,secondY,second,0);show();
	}
	int choice=0,hover=-1;
	while(!choice && !quit) {
		SciEvent input=poll();
		Common::Point mouse=g_sci->_gfxCursor->getPosition();
		int h=firstHit.contains(mouse)?1:secondHit.contains(mouse)?2:0;
		if(h!=hover) {hover=h;fill(x,firstY,choiceTextWidth,secondY+second.size()*lineHeight-firstY,5);paragraph(x,firstY,first,h==1?10:0);paragraph(x,secondY,second,h==2?10:0);show();}
		if(input.type==kSciEventMousePress) {
			if(firstHit.contains(input.mousePos))choice=1;
			if(secondHit.contains(input.mousePos))choice=2;
		}
		if(input.type==kSciEventKeyDown) {
			if(input.character=='1')choice=1;
			if(input.character=='2'||input.character==kSciKeyEsc)choice=2;
		}
		g_system->delayMillis(16);
	}
	g_system->getMixer()->stopHandle(voice);
	if(choice!=1 || quit)closePanel();
	if(choice==2 && !quit) {
		const char *reply="Okay. Have fun.";
		Common::String upper(reply);upper.toUppercase();
		int replyWidth=0;
		for(uint i=0;i<upper.size();++i)replyWidth+=font->getCharWidth((byte)upper[i]);
		const int width=replyWidth+2*inset,height=lineHeight+2*inset;
		const int replyLeft=(320-width)/2,replyTop=(200-height)/2;
		panel(replyLeft,replyTop,replyLeft+width,replyTop+height);
		text(replyLeft+inset,replyTop+inset,reply);show();
		const uint32 duration=play("1020-200-0-0-4",voice,Audio::Mixer::kSpeechSoundType);
		wait(duration?duration+250:2200);
		closePanel();
	}
	if(choice==1 && !quit) {
		// Fade the already displayed frame intact. Disposing the panel or
		// restoring background here would visibly remove actors before black.
		g_sci->_gfxScreen->fadeMazePlayfield(false);
		showMazeActors=false;
		closePanel(); // Hidden now that the fade has reached black.
		fill(0,10,320,190,0);show();
		g_sci->_gfxScreen->setPalette(palette,0,256,false);
		if(!quit){play("maze-metal",metal,Audio::Mixer::kSFXSoundType,220);wait(650);}
		if(!quit){play("maze-metal",metal,Audio::Mixer::kSFXSoundType,175);wait(230);}
		if(!quit){uint32 ow=play("1020-200-0-0-3",voice,Audio::Mixer::kSpeechSoundType,175);wait(MAX<uint32>(ow,900));}
		if(!quit){play("maze-metal",metal,Audio::Mixer::kSFXSoundType,140);wait(700);}
		if(!quit) {
			const char *closing="You emerge forty minutes older.";
			Common::String upper(closing);upper.toUppercase();
			int textWidth=0;for(uint i=0;i<upper.size();++i)textWidth+=font->getCharWidth((byte)upper[i]);
			const int padding=4,width=textWidth+2*padding,height=font->getHeight()+2*padding;
			const int closingLeft=(320-width)/2,closingTop=(200-height)/2;
			panel(closingLeft,closingTop,closingLeft+width,closingTop+height);
			text((320-textWidth)/2,closingTop+padding,closing);show();
			uint32 duration=play("1020-200-0-0-2",voice,Audio::Mixer::kSpeechSoundType);
			wait(duration?duration+250:4200);
		}
	}
	if(choice==1 && !quit) {
		// Black out BEFORE disposing the last panel, which restores its saved
		// background. Keep every native room-init palette update black too.
		fill(0,10,320,190,0);show();
		memcpy(s->sq5MazeFadePalette,palette,sizeof(palette));
		s->sq5MazeFadeReady=false;
		s->sq5MazeFadePending=true;
	}
	closePanel();
	g_system->getMixer()->stopHandle(voice);g_system->getMixer()->stopHandle(metal);
	g_sci->_gfxScreen->setPalette(palette,0,256,false);
	g_sci->_gfxScreen->bitsRestore(&background[0]);show();
	s->r_acc=oldAcc;
	if(quit)return;
	event.type=kSciEventNone;
	s->sq5MazeChoice=choice==1?3:1;
	if(choice==1) {
		// Native initialization builds deck 2, and the VM arrival hook selects
		// cell 26. sUpdateRoom handles the shield discovery and original score.
		s->variables[VAR_GLOBAL][133]=make_reg(0,2);
		reg_t room=make_reg(0,1020);
		invokeSelector(s,s->variables[VAR_GLOBAL][2],g_sci->getKernel()->findSelector("newRoom"),argc,argv,1,&room);
		s->r_acc=oldAcc;
	}
}

void sq5NavigationMenu(SciEvent &event, int argc, reg_t *argv) {
	if (!g_sci->isSQ5TrashTalkin() || !ConfMan.hasKey("sq5_voice_path")) return;
	const byte *galaxyArtPalette = g_sci->_tts->editionAsset("galaxyArtPalette", 192);
	if (!galaxyArtPalette) return;
	const byte *galaxyArtPixels = g_sci->_tts->editionAsset("galaxyArtPixels", 60800);
	if (!galaxyArtPixels) return;
	const byte *galaxySpritePalette = g_sci->_tts->editionAsset("galaxySpritePalette", 192);
	if (!galaxySpritePalette) return;
	const byte *galaxySprite0 = g_sci->_tts->editionAsset("galaxySprite0", 3108);
	if (!galaxySprite0) return;
	const byte *galaxySprite1 = g_sci->_tts->editionAsset("galaxySprite1", 3108);
	if (!galaxySprite1) return;
	const byte *galaxySprite2 = g_sci->_tts->editionAsset("galaxySprite2", 3108);
	if (!galaxySprite2) return;
	const byte *galaxySprite3 = g_sci->_tts->editionAsset("galaxySprite3", 3108);
	if (!galaxySprite3) return;
	const byte *galaxySprite4 = g_sci->_tts->editionAsset("galaxySprite4", 3108);
	if (!galaxySprite4) return;
	const byte *galaxySprite5 = g_sci->_tts->editionAsset("galaxySprite5", 3108);
	if (!galaxySprite5) return;
	const byte *galaxySprite6 = g_sci->_tts->editionAsset("galaxySprite6", 3108);
	if (!galaxySprite6) return;
	const byte *galaxySprite7 = g_sci->_tts->editionAsset("galaxySprite7", 3108);
	if (!galaxySprite7) return;
	const byte *galaxySprite8 = g_sci->_tts->editionAsset("galaxySprite8", 3108);
	if (!galaxySprite8) return;
	const byte *galaxySprite9 = g_sci->_tts->editionAsset("galaxySprite9", 3108);
	if (!galaxySprite9) return;
	const byte *galaxySprite10 = g_sci->_tts->editionAsset("galaxySprite10", 3108);
	if (!galaxySprite10) return;
	const byte *galaxySprite11 = g_sci->_tts->editionAsset("galaxySprite11", 3108);
	if (!galaxySprite11) return;
	const byte *galaxySprite12 = g_sci->_tts->editionAsset("galaxySprite12", 3108);
	if (!galaxySprite12) return;
	const byte *goliath1 = g_sci->_tts->editionAsset("goliath1", 1296);
	if (!goliath1) return;
	const byte *goliath2 = g_sci->_tts->editionAsset("goliath2", 1296);
	if (!goliath2) return;
	const byte *const galaxySprites[] = {galaxySprite0,galaxySprite1,galaxySprite2,galaxySprite3,galaxySprite4,galaxySprite5,galaxySprite6,galaxySprite7,galaxySprite8,galaxySprite9,galaxySprite10,galaxySprite11,galaxySprite12};
	EngineState *s = g_sci->getEngineState();
	SegManager *seg = s->_segMan;
	static bool handled = false;
	const SegmentId keypadSegment = seg->getScriptSegment(203);
	auto keypadObject = [&](const char *name) {
		for (const reg_t &object : seg->findObjectsByName(name))
			if (object.getSegment() == keypadSegment) return object;
		return NULL_REG;
	};
	reg_t script = keypadObject("keyStuff");
	reg_t pad = keypadObject("pad");
	// Wait for the original keypad cancellation to finish before using the
	// game's own Narrator and Droole portrait. No fabricated input dismissals.
	if (s->currentRoomNumber()!=201) {
		g_sci->_sq5ChartPendingRefusal=-1;
		g_sci->_sq5ChartFollowup=-1;
		g_sci->_sq5ChartFloActor=NULL_REG;
		g_sci->_sq5ChartFloArm=NULL_REG;
		g_sci->_sq5ChartRogerActor=NULL_REG;
	}
	bool chartFollowup=g_sci->_sq5ChartFollowup==-2;
	auto restoreChartFlo=[&]() {
		if(!g_sci->_sq5ChartRogerActor.isNull()&&seg->getObject(g_sci->_sq5ChartRogerActor)) {
			const reg_t actor=g_sci->_sq5ChartRogerActor;
			invokeSelector(s,actor,g_sci->getKernel()->findSelector("show"),argc,argv,0,nullptr);
			const reg_t loop[]={make_reg(0,g_sci->_sq5ChartRogerLoop)},cel[]={make_reg(0,g_sci->_sq5ChartRogerCel)};
			const reg_t pos[]={make_reg(0,g_sci->_sq5ChartRogerX),make_reg(0,g_sci->_sq5ChartRogerY)};
			invokeSelector(s,actor,g_sci->getKernel()->findSelector("startUpd"),argc,argv,0,nullptr);
			invokeSelector(s,actor,g_sci->getKernel()->findSelector("setLoop"),argc,argv,1,loop);
			invokeSelector(s,actor,g_sci->getKernel()->findSelector("setCel"),argc,argv,1,cel);
			invokeSelector(s,actor,g_sci->getKernel()->findSelector("posn"),argc,argv,2,pos);
			invokeSelector(s,actor,g_sci->getKernel()->findSelector("stopUpd"),argc,argv,0,nullptr);
			// Rebuild the static actor's saved background in this frame.
			uint16 signal=readSelectorValue(seg,actor,SELECTOR(signal));
			writeSelectorValue(seg,actor,SELECTOR(signal),(signal|kSignalNoUpdate|kSignalForceUpdate)&~(kSignalViewUpdated|kSignalStopUpdate));
			reg_t cast=readSelector(seg,s->variables[VAR_GLOBAL][5],g_sci->getKernel()->findSelector("elements"));
			g_sci->_gfxAnimate->kernelAnimate(cast,false,argc,argv);
		}
		g_sci->_sq5ChartRogerActor=NULL_REG;
		if(!g_sci->_sq5ChartFloActor.isNull()&&seg->getObject(g_sci->_sq5ChartFloActor)) {
			const reg_t loop[]={make_reg(0,g_sci->_sq5ChartFloLoop)},cel[]={make_reg(0,g_sci->_sq5ChartFloCel)};
			invokeSelector(s,g_sci->_sq5ChartFloActor,g_sci->getKernel()->findSelector("startUpd"),argc,argv,0,nullptr);
			invokeSelector(s,g_sci->_sq5ChartFloActor,g_sci->getKernel()->findSelector("setLoop"),argc,argv,1,loop);
			invokeSelector(s,g_sci->_sq5ChartFloActor,g_sci->getKernel()->findSelector("setCel"),argc,argv,1,cel);
			seg->getScript(seg->getScriptSegment(201))->getLocalsBegin()[73]=g_sci->_sq5ChartFloFacing;
			if(!g_sci->_sq5ChartFloArm.isNull()&&seg->getObject(g_sci->_sq5ChartFloArm))
				invokeSelector(s,g_sci->_sq5ChartFloArm,g_sci->getKernel()->findSelector("show"),argc,argv,0,nullptr);
			reg_t cast=readSelector(seg,s->variables[VAR_GLOBAL][5],g_sci->getKernel()->findSelector("elements"));
			g_sci->_gfxAnimate->kernelAnimate(cast,false,argc,argv);
			invokeSelector(s,g_sci->_sq5ChartFloActor,g_sci->getKernel()->findSelector("stopUpd"),argc,argv,0,nullptr);
		}
		g_sci->_sq5ChartFloActor=NULL_REG;
		g_sci->_sq5ChartFloArm=NULL_REG;
		g_system->updateScreen();
	};
	if(g_sci->_sq5ChartFollowup>=0) {
		for(const reg_t candidate:seg->findObjectsByName("floTalker")) {
			if(candidate.getSegment()!=seg->getScriptSegment(201))continue;
			if(readSelectorValue(seg,candidate,g_sci->getKernel()->findSelector("ticks"))==0xffff) {
				g_sci->_sq5ChartPendingRefusal=g_sci->_sq5ChartFollowup;
				g_sci->_sq5ChartFollowup=-2;
				restoreChartFlo();
				// Finish drawing the right-facing pose before starting Droole.
				event.type=kSciEventNone;
				return;
			}
			break;
		}
	}
	if (g_sci->_sq5ChartPendingRefusal>=0 &&
		(script.isNull() || readSelector(seg,s->variables[VAR_GLOBAL][2],g_sci->getKernel()->findSelector("script"))!=script)) {
		if(chartFollowup) g_sci->_sq5ChartFollowup=-1;
		const int index=g_sci->_sq5ChartPendingRefusal;
		g_sci->_sq5ChartPendingRefusal=-1;
		g_sci->_sq5ChartSpeakingRefusal=(index<(int)ARRAYSIZE(galaxyPlaces)||index==kGalaxyAlreadyHere)?index:-1;
		g_sci->_sq5ChartSpeakingSequence=chartFollowup?2:1;
		const reg_t params[]={make_reg(0,23),make_reg(0,0),make_reg(0,43),make_reg(0,chartFollowup?2:1)};
		const reg_t acc=s->r_acc;
		if(!chartFollowup&&index<(int)ARRAYSIZE(galaxyPlaces)&&galaxyPlaces[index].refusalSpeaker==22&&galaxyPlaces[index].followup[0]) {
			const reg_t roger=s->variables[VAR_GLOBAL][0];
			if(seg->getObject(roger)&&readSelectorValue(seg,roger,SELECTOR(view))==200) {
				g_sci->_sq5ChartRogerActor=roger;
				g_sci->_sq5ChartRogerLoop=readSelectorValue(seg,roger,SELECTOR(loop));
				g_sci->_sq5ChartRogerCel=readSelectorValue(seg,roger,SELECTOR(cel));
				g_sci->_sq5ChartRogerX=readSelectorValue(seg,roger,SELECTOR(x));
				g_sci->_sq5ChartRogerY=readSelectorValue(seg,roger,SELECTOR(y));
				// Script 201's native Flo-facing pose: loop 1 at (178,95).
				const reg_t loop[]={make_reg(0,1)},cel[]={NULL_REG},pos[]={make_reg(0,178),make_reg(0,95)};
				invokeSelector(s,roger,g_sci->getKernel()->findSelector("startUpd"),argc,argv,0,nullptr);
				invokeSelector(s,roger,g_sci->getKernel()->findSelector("setLoop"),argc,argv,1,loop);
				invokeSelector(s,roger,g_sci->getKernel()->findSelector("setCel"),argc,argv,1,cel);
				invokeSelector(s,roger,g_sci->getKernel()->findSelector("posn"),argc,argv,2,pos);
				invokeSelector(s,roger,g_sci->getKernel()->findSelector("stopUpd"),argc,argv,0,nullptr);
			}
			for(const reg_t actor:seg->findObjectsByName("flo")) {
				if(actor.getSegment()!=seg->getScriptSegment(201))continue;
				g_sci->_sq5ChartFloActor=actor;
				invokeSelector(s,actor,g_sci->getKernel()->findSelector("startUpd"),argc,argv,0,nullptr);
				// floTalker supplies facial animation, not a replacement
				// seated actor. Match script 201's turned pose before its
				// background is captured; hiding the actor loses her torso.
				g_sci->_sq5ChartFloLoop=readSelectorValue(seg,actor,SELECTOR(loop));
				g_sci->_sq5ChartFloCel=readSelectorValue(seg,actor,SELECTOR(cel));
				g_sci->_sq5ChartFloFacing=seg->getScript(seg->getScriptSegment(201))->getLocalsBegin()[73];
				for(const reg_t arm:seg->findObjectsByName("floArm")) {
					if(arm.getSegment()!=actor.getSegment())continue;
					if(!(readSelectorValue(seg,arm,SELECTOR(signal))&kSignalHidden)) {
						g_sci->_sq5ChartFloArm=arm;
						invokeSelector(s,arm,g_sci->getKernel()->findSelector("hide"),argc,argv,0,nullptr);
					}
					break;
				}
				reg_t zero[]={NULL_REG},one[]={make_reg(0,1)};
				invokeSelector(s,actor,g_sci->getKernel()->findSelector("setCycle"),argc,argv,1,zero);
				invokeSelector(s,actor,g_sci->getKernel()->findSelector("setLoop"),argc,argv,1,one);
				invokeSelector(s,actor,g_sci->getKernel()->findSelector("setCel"),argc,argv,1,one);
				seg->getScript(seg->getScriptSegment(201))->getLocalsBegin()[73]=make_reg(0,1);
				invokeSelector(s,actor,g_sci->getKernel()->findSelector("stopUpd"),argc,argv,0,nullptr);
				reg_t cast=readSelector(seg,s->variables[VAR_GLOBAL][5],g_sci->getKernel()->findSelector("elements"));
				g_sci->_gfxAnimate->kernelAnimate(cast,false,argc,argv);
				break;
			}
		}
		// Animate through SCI normally; restore Flo only after her talker
		// disposes, then let Droole deliver the next part of the reply.
		invokeSelector(s,s->variables[VAR_GLOBAL][91],g_sci->getKernel()->findSelector("say"),argc,argv,4,params);
		if(!chartFollowup&&index<(int)ARRAYSIZE(galaxyPlaces)&&galaxyPlaces[index].followup[0])
			g_sci->_sq5ChartFollowup=index;
		s->r_acc=acc;
		g_sci->_sq5ChartSpeakingRefusal=-1;
		event.type=kSciEventNone;
		return;
	}

	if (s->currentRoomNumber() != 201 || script.isNull() || pad.isNull() ||
		readSelectorValue(seg, script, SELECTOR(state)) != 1) {
		handled = false;
		g_sci->_sq5ChartBeepToSkip=NULL_REG;
		return;
	}
	if (handled) return;
	// Only intercept an ordinary input poll, after the pad has initialized.
	if (event.type == kSciEventQuit) return;
	reg_t digitObjects[5];
	for (int i=0;i<5;++i) {
		digitObjects[i]=keypadObject(Common::String::format("d%d",4-i).c_str());
		if (digitObjects[i].isNull()) return;
	}
	handled = true;
	loadGalaxyOverrides();
	auto global = [&](int n) { return s->variables[VAR_GLOBAL][n].toUint16(); };
	auto flag = [&](int n) { return (global(183 + n / 16) & (0x8000 >> (n % 16))) != 0; };
	const int here = global(113);
	const reg_t ship=seg->findObjectByName("eureka");
	const int shipLocation=ship.isNull()?here:readSelectorValue(seg,ship,g_sci->getKernel()->findSelector("curLocation"));
	const int shipState=ship.isNull()?-1:readSelectorValue(seg,ship,SELECTOR(state));
	auto alreadyHere=[&](const GalaxyPlace &p) {
		if(!strcmp(p.key,"goliath")) return shipState!=2 && (shipLocation==12||shipLocation==14);
		if(!strcmp(p.key,"academy")) return shipLocation==0&&shipState==0;
		return p.location>0 && shipState!=2 &&
			(shipLocation==p.location || (p.location==7&&shipLocation==8));
	};

	// Chart knowledge and travel clearance are separate. Story locations stay
	// hidden until their original script flags reveal them; leisure POIs are
	// charted, but never submitted to the game's coordinate validator.
	auto visible = [&](const GalaxyPlace &entry) {
		if (!strcmp(entry.key,"goliath")) return s->sq5GoliathLocated!=0;
		const GalaxyPlace &p=galaxyPlaces[entry.group];
		if (p.location==7) return flag(222)||here==7||here==8;
		if (p.location==6) return flag(33)||flag(94)||here==6;
		if (p.location==5) return flag(30)||flag(93)||here==5;
		return true;
	};
	auto allowed = [&](const GalaxyPlace &p) {
		if (!p.location) return false;
		if (p.location<=3) return flag(31)||here==p.location;
		if (p.location==4) return !flag(30)&&(global(127)>=3||here==4);
		return visible(p);
	};
	auto mission = [&](const GalaxyPlace &p) -> const char * {
		if (!strcmp(p.key,"goliath")) return "WD40 SENSOR FIX / INTERCEPT";
		if (!strcmp(p.key,"gingivitis") && s->sq5GoliathLocated) return "GOLIATH LOCATED";
		if (p.location>=1 && p.location<=3 && flag(31)) {
			// Script 208 counts the WD40 encounter as the Kiz assignment.
			// Flag 106 only queues the return conversation and is then cleared.
			const bool kizComplete = global(127) > int(flag(35)) + int(flag(36));
			const bool complete = p.location == 3 ? kizComplete : flag(p.location == 1 ? 35 : 36);
			return complete ? "ASSIGNMENT COMPLETE" : "RRS PICKUP";
		}
		if (p.location==4 && flag(30)) return "STATION DESTROYED";
		if (p.location==5 && !flag(93)) return "NEW: INVESTIGATE";
		if (p.location==6 && !flag(94)) return "NEW: DISTRESS SIGNAL";
		// Being elsewhere does not mean Genetix is unvisited. Its known
		// coordinates remain available after the fly mission and departure.
		return allowed(p) ? "COURSE AVAILABLE" : "CHARTED / NO ASSIGNMENT";
	};
	Common::Array<int> known;
	for (uint i=0;i<ARRAYSIZE(galaxyPlaces);++i) if (galaxyPlaces[i].group==(int)i && visible(galaxyPlaces[i])) known.push_back(i);
	int selected=-1, system=-1, inspected=-1;
	Port *port=g_sci->_gfxPorts->getPort();
	const Common::Rect area(0,10,320,200);
	Common::Rect local=area; local.translate(-port->left,-port->top);
	Common::Array<byte> background;
	background.resize(g_sci->_gfxScreen->bitsGetDataSize(area,1));
	g_sci->_gfxScreen->bitsSave(area,1,&background[0]);
	auto fill=[&](int x,int y,int w,int h,int c) {
		Common::Rect r(x,y,x+w,y+h); r.clip(area);
		if (!r.isEmpty()) { r.translate(-port->left,-port->top); g_sci->_gfxPaint16->fillRect(r,1,c); }
	};
	auto text=[&](int x,int y,const char *label,int color) {
		reg_t args[]={make_reg(0,100),make_reg(0,x-port->left),make_reg(0,y-port->top),make_reg(0,102),make_reg(0,color),make_reg(0,105),make_reg(0,4),make_reg(0,121)};
		g_sci->_gfxPaint16->kernelDisplay(label,0,8,args);
	};
	// Compact single-stroke 3x5 lettering for secondary map status.
	auto smallText=[&](int x,int y,const Common::String &label,int color) {
		static const char *glyphs[]={
			"010101111101101","110101110101110","011100100100011","110101101101110",
			"111100110100111","111100110100100","011100101101011","101101111101101",
			"111010010010111","001001001101010","101101110101101","100100100100111",
			"101111111101101","101111111111101","010101101101010","110101110100100",
			"010101101111011","110101110101101","011100010001110","111010010010010",
			"101101101101111","101101101101010","101101111111101","101101010101101",
			"101101010010010","111001010100111"};
		for(uint i=0;i<label.size();++i) {
			const char ch=label[i];
			if(ch>='A'&&ch<='Z') {
				const char *glyph=glyphs[ch-'A'];
				for(int row=0;row<5;++row)for(int col=0;col<3;++col)
					if(glyph[row*3+col]=='1')fill(x+col,y+row,1,1,color);
			} else if(ch=='/') {fill(x+2,y,1,1,color);fill(x+1,y+2,1,1,color);fill(x,y+4,1,1,color);}
			else if(ch==':') {fill(x+1,y+1,1,1,color);fill(x+1,y+3,1,1,color);}
			x+=4;
		}
	};
	GfxFont *font=g_sci->_gfxCache->getFont(4);
	auto width=[&](const Common::String &t) { int w=0; for (uint i=0;i<t.size();++i) w+=font->getCharWidth((byte)t[i]); return w; };
	// Own palette entries, excluding every color in the preserved status bar.
	bool used[256]={};
	for(int y=0;y<10;++y) for(int x=0;x<320;++x) used[g_sci->_gfxScreen->getVisual(x,y)]=true;
	byte savedPalette[768]; g_sci->_gfxScreen->grabPalette(savedPalette,0,256);
	const byte rgb[][3]={{0,0,0},{19,35,38},{55,84,87},{151,184,173},{80,255,104},{229,244,217},
		{70,37,20},{148,75,33},{233,155,67},{255,227,157},{19,57,64},{36,112,126},{95,192,185},
		{31,65,36},{72,130,62},{151,189,97},{79,82,99},{135,143,162},{199,208,211},{255,187,63},{255,72,65}};
	const byte bismolPalette[][3]={{36,27,43},{58,40,59},{82,55,77},{108,73,96},
		{135,92,116},{160,112,133},{182,134,150},{202,157,169},
		{219,181,188},{231,201,203},{241,218,214},{249,234,225}};
	int colors[ARRAYSIZE(rgb)], artColors[64], darkArtColors[64], spriteColors[64], bismolColors[12], slot=255;
	for(uint i=0;i<ARRAYSIZE(rgb);++i) { while(slot>0&&used[slot]) --slot; colors[i]=slot--; }
	for(int i=0;i<64;++i) {while(slot>0&&used[slot])--slot;artColors[i]=slot--;while(slot>0&&used[slot])--slot;darkArtColors[i]=slot--;}
	for(int i=0;i<64;++i) {while(slot>0&&used[slot])--slot;spriteColors[i]=slot--;}
	for(uint i=0;i<ARRAYSIZE(bismolPalette);++i) {while(slot>0&&used[slot])--slot;bismolColors[i]=slot--;}
	const int black=colors[0], faint=colors[1], dim=colors[2], bright=colors[3], green=colors[4], amber=colors[19], red=colors[20];
	auto disc=[&](int cx,int cy,int r,int base,int salt) {
		for(int y=-r;y<=r;++y) for(int x=-r;x<=r;++x) if(x*x+y*y<=r*r) {
			int shade=(x-y+r*2)*3/(r*4+1);
			if (((x*13+y*7+salt)&15)<3) shade=MAX(0,shade-1);
			fill(cx+x,cy+y,1,1,colors[base+CLIP(shade,0,base==16?2:3)]);
		}
	};
	auto ellipse=[&](int cx,int cy,int rx,int ry,int color) {
		for(int x=-rx;x<=rx;++x) {
			int y=(int)(ry*sqrt(1.0-(double)x*x/(rx*rx)));
			fill(cx+x,cy-y,1,1,color); fill(cx+x,cy+y,1,1,color);
		}
		for(int y=-ry;y<=ry;++y) {
			int x=(int)(rx*sqrt(1.0-(double)y*y/(ry*ry)));
			fill(cx-x,cy+y,1,1,color);fill(cx+x,cy+y,1,1,color);
		}
	};
	auto sun=[&](int cx,int cy,int radius) {
		for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x) {
			int d=x*x+y*y;if(d>radius*radius)continue;
			int shade=d<radius*radius/5?3:d<radius*radius/2?2:1;
			fill(cx+x,cy+y,1,1,colors[6+shade]);
		}
	};
	struct Star {int x,y,c;}; Common::Array<Star> stars;
	uint32 seed=0x5a17c9e3;
	auto randomStar=[&]() {seed^=seed<<13; seed^=seed>>17; seed^=seed<<5; return seed;};
	for(int i=0;i<240;++i) stars.push_back({4+(int)(randomStar()%312),29+(int)(randomStar()%150),i%9?faint:dim});
	Audio::SoundHandle voiceHandle;
	Common::String description;
	uint32 spokenAt=0, duration=0, hoverAt=0;
	bool refusal=false, narrated=false;
	int pageOverride=-1, displayedPage=0, totalPages=1;
	reg_t selectionSound=NULL_REG;
	const char *pendingBody=nullptr,*pendingVoice=nullptr;
	uint32 beepStarted=0;
	auto stopSelectionSound=[&]() {
		if(!selectionSound.isNull())g_sci->_soundCmd->processStopSound(selectionSound,false);
		selectionSound=NULL_REG;
		pendingBody=pendingVoice=nullptr;
	};
	auto selectionBeep=[&]() {
		stopSelectionSound();
		g_system->getMixer()->stopHandle(voiceHandle);
		selectionSound=keypadObject("soundFX");
		if(!selectionSound.isNull()) {
			reg_t arg=selectionSound;
			g_sci->_soundCmd->kDoSoundInit(s,1,&arg);
			g_sci->_soundCmd->processPlaySound(selectionSound,false);
		}
		beepStarted=g_system->getMillis();
	};
	auto speak=[&](const char *body,const char *id) {
		stopSelectionSound();
		g_system->getMixer()->stopHandle(voiceHandle);
		description=body; spokenAt=g_system->getMillis(); duration=description.size()*38; pageOverride=-1;
		Common::File *f=new Common::File();
		if (f->open(Common::FSNode(ConfMan.getPath("sq5_voice_path").join(Common::String(id)+".wav")))) {
			Audio::SeekableAudioStream *stream=Audio::makeWAVStream(f,DisposeAfterUse::YES);
			if(stream) {duration=MAX<uint32>(1,stream->getLength().msecs()); g_system->getMixer()->playStream(Audio::Mixer::kSpeechSoundType,&voiceHandle,stream);}
		} else delete f;
	};
	int lockElapsed=-1;
	uint32 selectionAt=0;
	bool courseAttempt=false;
	int dimSpriteColors[64];
	for(int i=0;i<64;++i) {
		int best=0, distance=0x7fffffff;
		for(int j=0;j<64;++j) {
			int d=0;for(int c=0;c<3;++c){int v=galaxySpritePalette[i*3+c]*45/100-galaxySpritePalette[j*3+c];d+=v*v;}
			if(d<distance){distance=d;best=j;}
		}
		dimSpriteColors[i]=spriteColors[best];
	}
	auto orbitRadius=[&](int index) {return 40+15*galaxyPlaces[index].orbit;};
	auto orbitHeight=[&](int index) {return 8+2*galaxyPlaces[index].orbit;};
	auto planetPosition=[&](int index) {
		const auto &p=galaxyPlaces[index];
		const int rx=orbitRadius(index),ry=orbitHeight(index);
		const int halfWidth=p.size/2,halfHeight=(p.size*42/74)/2;
		Common::Point pos;
		// Adjust orbital phase, never Cartesian coordinates: the body remains
		// on its own ellipse while its entire sprite clears both suns and UI.
		for(int step=0;step<360;++step) {
			double angle=(p.angle+step)*3.141592653589793/180;
			pos=Common::Point(154+(int)(rx*cos(angle)),58+(int)(ry*sin(angle)));
			Common::Rect bounds(pos.x-halfWidth,pos.y-halfHeight,pos.x+halfWidth+1,pos.y+halfHeight+1);
			if(bounds.left<6||bounds.right>314||bounds.top<28||bounds.bottom>83)continue;
			if(bounds.intersects(Common::Rect(141,45,168,72)))continue;
			if(galaxyPlaces[p.group].suns==2&&bounds.intersects(Common::Rect(165,55,184,74)))continue;
			break;
		}
		return pos;
	};
	auto bodyPosition=[&](int index) {
		if (!strcmp(galaxyPlaces[index].key,"goliath")) {
			const auto target=planetPosition(galaxyPlaces[index].group);
			// Park just outside the planet's silhouette, on its outer side.
			// Drawing and hit testing share this position; facing follows target.
			const int separation=galaxyPlaces[galaxyPlaces[index].group].size/2+15;
			return Common::Point(CLIP(target.x+(target.x<154?-separation:separation),16,303),
								 CLIP(target.y+(target.y>=58?-9:9),33,77));
		}
		return planetPosition(index);
	};

	const uint32 atlasOpenedAt=g_system->getMillis();
	auto draw=[&]() {
		const uint32 introElapsed=g_system->getMillis()-atlasOpenedAt;
		const int fade=MIN<int>(255,introElapsed*255/180);
		for(int i=0;i<64;++i) {byte color[3];for(int k=0;k<3;++k)color[k]=galaxyArtPalette[i*3+k]*fade/255;g_sci->_gfxScreen->setPalette(color,artColors[i],1,false);}
		fill(0,10,320,190,black);
		for(int y=27;y<184;++y) for(int x=0;x<320;++x) {
			int c=galaxyArtPixels[(y-10)*320+x];
			g_sci->_gfxScreen->putPixel(x,y,1,system<0?artColors[c]:darkArtColors[c],0,0);
		}
		fill(6,25,308,1,dim); fill(6,185,308,1,dim);
		text(8,14,system<0?"STARCON / GALACTIC ATLAS":"< GALAXY",green);
		text(282,14,"CLOSE",bright);
		if(system<0) {
			int revealIndex=0;
			for(int index:known) {
				const uint32 revealAt=180+(revealIndex++*140/MAX<int>(1,known.size()));
				if(introElapsed<revealAt)continue;
				const auto &p=galaxyPlaces[index]; int c=index==selected?green:bright;
				// Rounded green marker drawn directly over the galaxy, with no backing box.
				for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
					if(dx*dx+dy*dy<=5)fill(p.x+dx,p.y+dy,1,1,green);
				const char *status=mission(p);
				if(!strcmp(status,"RRS PICKUP")) ellipse(p.x,p.y,7,6,red);
				if(!strcmp(status,"GOLIATH LOCATED")) {
					ellipse(p.x,p.y,7,6,red);
				}
				if(!strncmp(status,"NEW:",4)) {text(MIN(p.x+6,289),p.y+3,"NEW",green);}
			}
			if(selected>=0) {
				const auto &p=galaxyPlaces[selected]; Common::String title=p.system;
				const int n=MIN<int>(title.size(),1+(g_system->getMillis()-hoverAt)/22);
				Common::String status=mission(p);
				const int boxWidth=MAX(width(title),(int)status.size()*4-1);
				const int lineHeight=font->getHeight()+2;
				const int x=CLIP(p.x+9+boxWidth>312?p.x-9-boxWidth:p.x+9,8,312-boxWidth);
				const int y=CLIP(p.y-12,28,182-2*lineHeight);
				const int elapsed=(int)(g_system->getMillis()-hoverAt);
				const int statusChars=CLIP((elapsed-(int)title.size()*22)/14,0,(int)status.size());
				fill(x-2,y-1,boxWidth+4,lineHeight+7,black);
				text(x,y,title.substr(0,n).c_str(),green);
				smallText(x,y+lineHeight,status.substr(0,statusChars),bright);
			}
			text(8,190,"SELECT A SYSTEM",bright);
		} else {
			const auto &sys=galaxyPlaces[system];
			text(82,14,sys.system,green);
			// Only occupied orbits are drawn. Body positions use these exact radii.
			bool drawnOrbits[7]={false};
			for(uint i=0;i<ARRAYSIZE(galaxyPlaces);++i) {
				const auto &body=galaxyPlaces[i];
				if(body.group!=system||!visible(body)||!strcmp(body.key,"goliath")||drawnOrbits[body.orbit])continue;
				drawnOrbits[body.orbit]=true;
				ellipse(154,58,orbitRadius(i),orbitHeight(i),dim);
			}
			sun(154,58,system%3==0?9:7);
			if(sys.suns==2)sun(174,64,6);
			for(uint index=0;index<ARRAYSIZE(galaxyPlaces);++index) {
				const auto &body=galaxyPlaces[index];if(body.group!=system||!visible(body))continue;
				auto pos=bodyPosition(index);int bw=body.size,bh=bw*42/74;
				if (!strcmp(body.key,"goliath")) {
					const auto target=planetPosition(body.group);
					// Front view points down-left; rear view points up-right.
					// Their horizontal mirrors supply the other two directions.
					const bool down=target.y>=pos.y;
					const bool flip=down ? target.x>pos.x : target.x<pos.x;
					const byte *sprite=down?goliath1:goliath2;
					for(int y=0;y<bh;++y)for(int x=0;x<bw;++x) {
						const int sx=x*48/bw,sy=y*27/bh;
						int c=sprite[sy*48+(flip?47-sx:sx)];
						if(c)fill(pos.x-bw/2+x,pos.y-bh/2+y,1,1,spriteColors[c]);
					}
					continue;
				}
				auto debrisRing=[&](bool front) {
					if(!(body.features&2))return;
					uint32 seed=0x735ac91;
					for(int r=0;r<85;++r) {
						seed=seed*1664525+1013904223;
						double a=(seed>>8)*6.2831853/16777216.0;
						if((sin(a)>=0)!=front)continue;
						seed=seed*1664525+1013904223;
						double radius=bw*0.56+((seed>>16)%4);
						fill(pos.x+(int)(radius*cos(a)),pos.y+(int)(radius*0.32*sin(a)),1,1,r%5==0?bright:dim);
					}
				};
				debrisRing(false);
				if(index==1&&flag(30)) {
					// Stable scattered fragments, not modular diagonal stripes.
					uint32 debrisSeed=0x51aceb4;
					for(int i=0;i<52;++i) {
						debrisSeed=debrisSeed*1664525+1013904223;
						int dx=(int)((debrisSeed>>16)%31)-15;
						debrisSeed=debrisSeed*1664525+1013904223;
						int dy=(int)((debrisSeed>>16)%21)-10;
						if(dx*dx*100+dy*dy*225>22500)continue;
						fill(pos.x+dx,pos.y+dy,i%9==0?2:1,i%13==0?2:1,i%7==0?bright:dim);
					}
				} else if(!strcmp(body.key,"parole")) {
					// A blue ice world: marbled white clouds, glacial cyan and a shaded limb.
					const bool emphasized=(int)index==selected||(int)index==inspected;
					const double radius=bh*0.49;
					auto blueRings=[&](bool front) {
						// Inclined icy rings; back half is occluded by the globe.
						for(int y=-bh;y<=bh;++y)for(int x=-bw;x<=bw;++x) {
							const double u=x*0.94-y*0.342,v=x*0.342+y*0.94;
							if((v>=0)!=front)continue;
							const double r=sqrt(u*u+v*v*9.0);
							if(r<radius*1.45||r>radius*2.65)continue;
							if(r>radius*2.05&&r<radius*2.18)continue;
							const double band=0.65+0.35*sin(r*3.2);
							int target[3]={(int)(70+85*band),(int)(145+80*band),250};
							if(!emphasized)for(int k=0;k<3;++k)target[k]=target[k]*65/100;
							int best=1,distance=0x7fffffff;
							for(int j=1;j<64;++j){int d=0;for(int k=0;k<3;++k){int delta=target[k]-galaxySpritePalette[j*3+k];d+=delta*delta;}if(d<distance){distance=d;best=j;}}
							fill(pos.x+x,pos.y+y,1,1,spriteColors[best]);
						}
					};
					blueRings(false);
					for(int y=-bh/2;y<=bh/2;++y)for(int x=-bh/2;x<=bh/2;++x) {
						const double nx=x/radius,ny=y/radius,r2=nx*nx+ny*ny;
						if(r2>1.0)continue;
						const double z=sqrt(1.0-r2);
						const double cloud=sin(ny*15+nx*5+sin(nx*8)*1.2)+0.5*sin(ny*29-nx*11);
						const double light=0.22+0.78*MAX(0.0,-nx*0.40-ny*0.30+z*0.85);
						int target[3]={30,125,230};
						if(cloud>0.45){target[0]=180;target[1]=225;target[2]=250;}
						else if(cloud>-0.25){target[0]=65;target[1]=185;target[2]=235;}
						if(r2>0.85&&nx<0){target[0]=140;target[1]=220;target[2]=255;}
						for(int k=0;k<3;++k)target[k]=(int)(target[k]*light*(emphasized?1.0:0.65));
						int best=1,distance=0x7fffffff;
						for(int j=1;j<64;++j){int d=0;for(int k=0;k<3;++k){int delta=target[k]-galaxySpritePalette[j*3+k];d+=delta*delta;}if(d<distance){distance=d;best=j;}}
						fill(pos.x+x,pos.y+y,1,1,spriteColors[best]);
					}
					blueRings(true);
				} else if(!strcmp(body.key,"lastcall")) {
					// A compact gas giant with curved cloud belts and a shaded limb.
					// Native pixels stay crisp; no recoloring of a rocky shared sprite.
					const bool emphasized=(int)index==selected||(int)index==inspected;
					const double ry=bh*0.48,rx=ry*1.2;
					for(int y=-bh/2;y<=bh/2;++y)for(int x=-bw/2;x<=bw/2;++x) {
						const double nx=x/rx,ny=y/ry,r2=nx*nx+ny*ny;
						if(r2>1.0)continue;
						const double z=sqrt(1.0-r2);
						const double latitude=ny+0.10*sin(nx*4.0+ny*5.0)+0.035*sin(nx*11.0-ny*7.0);
						const double bands=sin(latitude*23.0)+0.35*sin(latitude*47.0+nx*3.0);
						const double light=MAX(0.0,-nx*0.42-ny*0.32+z*0.83);
						double shade=2.0+light*6.8+bands*0.85;
						// Fine ordered dither blends the chalk-pink and ivory cloud layers.
						shade+=((x+y)&1)?0.22:-0.22;
						if(!emphasized)shade*=0.55;
						fill(pos.x+x,pos.y+y,1,1,bismolColors[CLIP((int)shade,0,11)]);
					}
				} else {
					const byte *sprite=galaxySprites[body.sprite];
					for(int y=0;y<bh;++y)for(int x=0;x<bw;++x) {
						int c=sprite[(y*42/bh)*74+x*74/bw];
						if(c) {
							const bool emphasized=(int)index==selected||(int)index==inspected;
							int color=emphasized?spriteColors[c]:dimSpriteColors[c];
							if(!strcmp(body.key,"morella")||!strcmp(body.key,"thrakus")||!strcmp(body.key,"pallor")) {
								const byte *source=galaxySpritePalette+c*3;
								int lum=(source[0]*54+source[1]*183+source[2]*19)/256;
								const bool ice=!strcmp(body.key,"morella");
								int target[3]={ice?35+lum*80/100:lum*43/100,
									ice?48+lum*80/100:24+lum*83/100,
									ice?65+lum*74/100:lum*36/100};
								if(!strcmp(body.key,"pallor")) {target[0]=55+lum*78/100;target[1]=54+lum*77/100;target[2]=51+lum*75/100;}
								if(!emphasized)for(int k=0;k<3;++k)target[k]=target[k]*45/100;
								int best=1,bestDistance=0x7fffffff;
								for(int j=1;j<64;++j) {
									int distance=0;
									for(int k=0;k<3;++k){int d=target[k]-galaxySpritePalette[j*3+k];distance+=d*d;}
									if(distance<bestDistance){bestDistance=distance;best=j;}
								}
								color=spriteColors[best];
							}
							g_sci->_gfxScreen->putPixel(pos.x-bw/2+x,pos.y-bh/2+y,1,color,0,0);
						}
					}
				}
				debrisRing(true);
				for(int m=0;m<body.moons;++m) {
					// Moon-rich worlds use a compact satellite belt, not nine
					// expanding rings that would cover adjacent destinations.
					const int mx=body.moons>3?bw/2+5:bw/3+4+m*4;
					const int my=body.moons>3?bh/2+3:bh/2+2+m*2;
					// Satellites remain visible without planet-centered orbit guides.
					double angle=(body.moons>3?15+m*360.0/body.moons:35+m*130)*3.141592653589793/180;
					disc(pos.x+(int)(mx*cos(angle)),pos.y+(int)(my*sin(angle)),1,16,m);
				}
			}
			// Draw selection overlays after every planet, ring and moon.
			// Nearby worlds must never occlude the selected body's brackets.
			for(uint index=0;index<ARRAYSIZE(galaxyPlaces);++index) {
				const auto &body=galaxyPlaces[index];if(body.group!=system||!visible(body))continue;
				auto pos=bodyPosition(index);int bw=body.size,bh=bw*42/74;
				const int clickElapsed=selectionAt?int(g_system->getMillis()-selectionAt):1000;
				const bool animating=(int)index==inspected&&(lockElapsed>=0||clickElapsed<200);
				if((int)index==selected||animating) {
					int rx=bw/2+3,ry=bh/2+3;
					if(animating){int t=MAX(0,200-(lockElapsed>=0?lockElapsed:clickElapsed));int extra=8*t*t/(200*200);rx+=extra;ry+=extra;}
					for(int a=-1;a<=1;a+=2)for(int b=-1;b<=1;b+=2){int x=pos.x+a*rx,y=pos.y+b*ry;fill(a<0?x:x-3,y,4,1,green);fill(x,b<0?y:y-3,1,4,green);}
				}

			}
			int title=selected>=0?selected:inspected;
			if(title>=0)text(CLIP(160-width(galaxyPlaces[title].name)/2,8,312-width(galaxyPlaces[title].name)),85,galaxyPlaces[title].name,green);
			fill(0,93,320,92,black);
			fill(6,96,308,1,dim);
			text(8,99,refusal?"DROOLE / HELM":"DESTINATION FILE",refusal?amber:green);
			if(!description.empty()) {
				Common::Array<Common::String> lines; Common::String row,word;
				for(uint i=0;i<=description.size();++i) {
					char c=i<description.size()?description[i]:' ';
					if(c!=' ') word+=c;
					else if(!word.empty()) {Common::String next=row.empty()?word:row+" "+word;if(width(next)>300&&!row.empty()){lines.push_back(row);row=word;}else row=next;word.clear();}
				}
				if(!row.empty()) lines.push_back(row);
				const int lineHeight=font->getHeight()+1;
				const int perPage=MAX(1,73/lineHeight),pages=(lines.size()+perPage-1)/perPage;
				int chars=MIN<uint32>(description.size(),(uint64)(g_system->getMillis()-spokenAt)*description.size()/MAX<uint32>(1,duration));
				int line=0,offset=0;
				while(line<(int)lines.size()-1&&offset+(int)lines[line].size()<chars){offset+=lines[line].size()+1;++line;}
				int page=pageOverride>=0?MIN(pageOverride,pages-1):line/perPage;
				displayedPage=page;totalPages=pages;
				int consumed=0;
				for(int i=0;i<(int)lines.size();++i) {
					int count=lines[i].size();
					if(i/perPage==page&&count) text(8,110+(i%perPage)*lineHeight,lines[i].substr(0,count).c_str(),green);
					consumed+=lines[i].size()+1;
				}
				text(148,190,Common::String::format("< %d/%d >",page+1,pages).c_str(),bright);
			} else text(8,113,"CLICK THE DESTINATION TO VIEW ITS FILE",dim);
			if(narrated) text(8,190,"REPLAY",bright);
			fill(211,187,104,12,narrated?(selected==-2?green:dim):faint);fill(212,188,102,10,black);
			text(217,190,"LAY IN A COURSE",narrated?green:dim);
		}
		g_sci->_gfxPaint16->bitsShow(local);g_system->updateScreen();
	};
	auto hit=[&](Common::Point mouse) {
		if(system>=0) {
			if(narrated&&Common::Rect(211,187,315,199).contains(mouse)) return -2;
			for(uint i=0;i<ARRAYSIZE(galaxyPlaces);++i)if(galaxyPlaces[i].group==system&&visible(galaxyPlaces[i])) {
				auto pos=bodyPosition(i);int hw=!strcmp(galaxyPlaces[i].key,"goliath")?10:MAX(8,galaxyPlaces[i].size/3),hh=!strcmp(galaxyPlaces[i].key,"goliath")?7:MAX(6,galaxyPlaces[i].size*21/74);if(Common::Rect(pos.x-hw,pos.y-hh,pos.x+hw+1,pos.y+hh+1).contains(mouse))return (int)i;
			}
			return -1;
		}
		for(int index:known) {const auto &p=galaxyPlaces[index];if(Common::Rect(p.x-6,p.y-6,p.x+7,p.y+7).contains(mouse))return index;}
		return -1;
	};
	struct ChartAudioScope {bool &value;bool previous;ChartAudioScope(bool &v):value(v),previous(v){value=true;}~ChartAudioScope(){value=previous;}} chartAudio(g_sci->_sq5ChartKeepsAudio);
	PauseToken navigationPause=g_sci->pauseEngine();
	for(uint i=0;i<ARRAYSIZE(rgb);++i) g_sci->_gfxScreen->setPalette(rgb[i],colors[i],1,false);
	for(uint i=0;i<ARRAYSIZE(bismolPalette);++i)g_sci->_gfxScreen->setPalette(bismolPalette[i],bismolColors[i],1,false);
	for(int i=0;i<64;++i) {
		g_sci->_gfxScreen->setPalette(galaxySpritePalette+i*3,spriteColors[i],1,false);
		g_sci->_gfxScreen->setPalette(galaxyArtPalette+i*3,artColors[i],1,false);
		byte dark[3];for(int c=0;c<3;++c)dark[c]=galaxyArtPalette[i*3+c]/8;
		g_sci->_gfxScreen->setPalette(dark,darkArtColors[i],1,false);
	}

	Common::Point lastMouse(-1,-1);
	int action=0;
	auto back=[&]() {stopSelectionSound();system=-1;selected=-1;inspected=-1;description.clear();refusal=false;narrated=false;g_system->getMixer()->stopHandle(voiceHandle);};
	auto choose=[&]() {
		if(system<0) {selectionBeep();system=galaxyPlaces[selected].group;selected=-1;inspected=-1;description.clear();refusal=false;narrated=false;lastMouse=Common::Point(-1,-1);return;}
		if(inspected<0)return;
		courseAttempt=true;
		const auto &p=galaxyPlaces[inspected];
		if(alreadyHere(p)) {g_sci->_sq5ChartPendingRefusal=kGalaxyAlreadyHere;action=2;return;}
		if(allowed(p)) {selected=inspected;action=1;return;}
		g_sci->_sq5ChartPendingRefusal=(p.location==4&&flag(30))?ARRAYSIZE(galaxyPlaces):inspected;
		action=2;
	};
	auto narrationBody=[&](int index) -> const char * {
		const auto &p=galaxyPlaces[index];
		return p.location==4&&flag(30)&&p.destroyedDescription[0]?p.destroyedDescription:p.description;
	};
	auto narrationVoice=[&](int index) -> const char * {
		const auto &p=galaxyPlaces[index];
		return p.location==4&&flag(30)&&p.destroyedVoice[0]?p.destroyedVoice:p.voice;
	};
	auto inspect=[&]() {
		if(system<0){choose();return;}
		if(selected>=0){
			inspected=selected;narrated=true;selectionAt=g_system->getMillis();selectionBeep();
			pendingBody=narrationBody(inspected);pendingVoice=narrationVoice(inspected);
			description=pendingBody;pageOverride=0;
		}
	};
	while(!action&&!g_sci->shouldQuit()&&s->abortScriptProcessing==kAbortNone) {
		SciEvent input=g_sci->getEventManager()->getSciEvent(kSciEventAny);
		if(input.type==kSciEventQuit){event=input;break;}
		Common::Point mouse=g_sci->_gfxCursor->getPosition();
		if(mouse!=lastMouse){int next=hit(mouse);if(next!=selected){selected=next;hoverAt=g_system->getMillis();}lastMouse=mouse;}
		if(input.type==kSciEventKeyDown) {
			if(input.character==kSciKeyEsc) {if(system>=0)back();else action=2;}
			else if(input.character==kSciKeyEnter&&selected>=0)inspect();
			else if(input.character==kSciKeyEnter&&selected==-2&&narrated)choose();
			else if(system>=0&&(input.character==kSciKeyUp||input.character==kSciKeyDown)) pageOverride=CLIP(displayedPage+(input.character==kSciKeyUp?-1:1),0,totalPages-1);
			else if(input.character==kSciKeyTab||input.character==kSciKeyRight||input.character==kSciKeyLeft) {
				if(system>=0) {
					Common::Array<int> choices;for(uint i=0;i<ARRAYSIZE(galaxyPlaces);++i)if(galaxyPlaces[i].group==system&&visible(galaxyPlaces[i]))choices.push_back(i);
					if(narrated)choices.push_back(-2);
					int n=-1;for(uint i=0;i<choices.size();++i)if(choices[i]==selected)n=i;
					selected=choices[(n+1)%choices.size()];
				}
				else {int row=-1;for(uint i=0;i<known.size();++i)if(known[i]==selected)row=i;row=(row+(input.character==kSciKeyLeft?known.size()-1:1))%known.size();selected=known[row];hoverAt=g_system->getMillis();}
			}
		}
		if(input.type==kSciEventMousePress) {
			auto m=input.mousePos;
			if(m.y>=10&&m.y<25&&m.x>=275) action=2;
			else if(system>=0&&m.y>=10&&m.y<25&&m.x<78)back();
			else if(system>=0&&m.y>=186) {
				if(m.x<68&&narrated){refusal=false;speak(narrationBody(inspected),narrationVoice(inspected));}
				else if(m.x>=211&&narrated) choose();
				else if(m.x>=140&&m.x<210){pageOverride=CLIP(displayedPage+(m.x<173?-1:1),0,totalPages-1);}
			} else {selected=hit(m);if(selected>=0)inspect();}
		}
		// Keep input responsive. A new selection cancels the previous queued
		// narration; closing/back/replay also clears it. Cap the click chirp at
		// 120 ms; speak() stops it before starting voice, without an extra gap.
		if(!action&&pendingBody&&(selectionSound.isNull()||
			!g_sci->_soundCmd->isSoundPlaying(selectionSound)||g_system->getMillis()-beepStarted>=120)) {
			const char *body=pendingBody,*id=pendingVoice;
			speak(body,id);
		}
		draw();g_system->delayMillis(20);
	}
	g_system->getMixer()->stopHandle(voiceHandle);
	stopSelectionSound();
	if(courseAttempt&&!g_sci->shouldQuit()&&s->abortScriptProcessing==kAbortNone) {
		g_sci->_sq5ChartBeepToSkip=NULL_REG;
		const reg_t confirmationSound=keypadObject("soundFX");
		if(!confirmationSound.isNull()) {reg_t soundArg=confirmationSound;g_sci->_soundCmd->kDoSoundInit(s,1,&soundArg);g_sci->_soundCmd->processPlaySound(confirmationSound,false);g_sci->_sq5ChartBeepToSkip=confirmationSound;}
		uint32 started=g_system->getMillis();
		while(g_system->getMillis()-started<260&&!g_sci->shouldQuit()&&s->abortScriptProcessing==kAbortNone) {
			lockElapsed=g_system->getMillis()-started;draw();
			SciEvent input=g_sci->getEventManager()->getSciEvent(kSciEventAny);if(input.type==kSciEventQuit){event=input;break;}g_system->delayMillis(16);
		}
	}
	g_sci->_gfxScreen->setPalette(savedPalette,0,256,false);
	g_sci->_gfxScreen->bitsRestore(&background[0]);g_sci->_gfxPaint16->bitsShow(local);
	if(event.type==kSciEventQuit||g_sci->shouldQuit()||s->abortScriptProcessing!=kAbortNone){handled=false;return;}
	// Original game scripts still own course validation and lite-speed travel.
	if(action==1) for(int i=0;i<5;++i) writeSelectorValue(seg,digitObjects[i],SELECTOR(loop),galaxyPlaces[inspected].coordinates[i]-'0');
	event.type=kSciEventMousePress;event.modifiers=kSciKeyModNone;event.character=0;
	event.mousePos=Common::Point(readSelectorValue(seg,pad,SELECTOR(x))+(action==1?22:60),readSelectorValue(seg,pad,SELECTOR(y))+82+port->top);
}


OptionsWidget::OptionsWidget(GuiObject *boss, const Common::String &name, const Common::String &domain) :
		OptionsContainerWidget(boss, name, "SciGameOptionsDialog", domain) {
	_guiOptions = ConfMan.get("guioptions", domain);

	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			_checkboxes[entry->option.configOption] = new GUI::CheckboxWidget(widgetsBoss(), _dialogLayout + "." + entry->option.configOption, _(entry->option.label), _(entry->option.tooltip));

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions)) {
			GUI::StaticTextWidget *textWidget = new GUI::StaticTextWidget(widgetsBoss(), _dialogLayout + "." + entry->configOption + "_desc", _(entry->label), _(entry->tooltip));
			textWidget->setAlign(Graphics::kTextAlignRight);

			_popUps[entry->configOption] = new GUI::PopUpWidget(widgetsBoss(), _dialogLayout + "." + entry->configOption);

			for (uint i = 0; entry->items[i].label; ++i)
				_popUps[entry->configOption]->appendEntry(_(entry->items[i].label), entry->items[i].configValue);
		}
}

void OptionsWidget::defineLayout(GUI::ThemeEval &layouts, const Common::String &layoutName, const Common::String &overlayedLayout) const {
	layouts.addDialog(layoutName, overlayedLayout);
	layouts.addLayout(GUI::ThemeLayout::kLayoutVertical).addPadding(0, 0, 0, 0);

	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		layouts.addWidget(entry->option.configOption, "Checkbox");

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry) {
		layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, 0);
		layouts.addWidget(Common::String(entry->configOption) + "_desc", "OptionsLabel");
		layouts.addWidget(entry->configOption, "PopUp").closeLayout();
	}

	layouts.closeLayout().closeDialog();
}

void OptionsWidget::load() {
	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			_checkboxes[entry->option.configOption]->setState(ConfMan.getBool(entry->option.configOption, _domain));

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			_popUps[entry->configOption]->setSelectedTag(ConfMan.getInt(entry->configOption, _domain));

	// If the deprecated native_fb01 option is set, use it to set midi_mode
	if (ConfMan.hasKey("native_fb01", _domain) && ConfMan.getBool("native_fb01", _domain))
		_popUps["midi_mode"]->setSelectedTag(kMidiModeFB01);
}

bool OptionsWidget::save() {
	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			ConfMan.setBool(entry->option.configOption, _checkboxes[entry->option.configOption]->getState(), _domain);

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			ConfMan.setInt(entry->configOption, _popUps[entry->configOption]->getSelectedTag(), _domain);

	// Remove deprecated option
	ConfMan.removeKey("native_fb01", _domain);

	return true;
}

} // End of namespace Sci
