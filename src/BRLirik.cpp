#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#ifdef NOGDI
#undef NOGDI
#endif
#include <windows.h>
#include <wingdi.h>
#include <objidl.h>
#include <ole2.h>
#include <gdiplus.h>
#include <commdlg.h>

#include "BRLirik.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <set>
#include <sstream>

#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"comdlg32.lib")
using namespace Gdiplus;
using namespace ffglex;

namespace {
enum Param:FFUInt32{
  P_ROLE,P_INPUT_MODE,P_EDIT,P_BROWSE_PNG,P_PNG_FIT,P_PNG_SCALE,
  P_PREVIOUS,P_NEXT,P_GOTO,P_GOTO_NUMBER,P_RESET,P_SHOW_PREVIEW,P_BLANK_EDGES,P_STATUS,
  P_TRANSITION,P_TRANSITION_DURATION,P_TRANSITION_SMOOTH,P_TRANSITION_LOCK,
  P_STACK_SCALE,P_STACK_OPACITY,P_STACK_GAP,
  P_MOTION,P_MOTION_AMOUNT,P_MOTION_SPEED,
  P_FONT_NAME,P_FONT_LIST,P_REFRESH_FONTS,P_FONT_SIZE,P_AUTO_FIT,P_ALIGN,P_VALIGN,P_LINE_SPACING,P_POSITION_PRESET,
  P_TEXT_H,P_TEXT_S,P_TEXT_B,P_TEXT_A,P_TEXT_FILL,
  P_GRADIENT,P_GRADIENT_DIRECTION,P_GRADIENT_H,P_GRADIENT_S,P_GRADIENT_B,P_GRADIENT_A,
  P_TEXT_STROKE,P_TEXT_STROKE_WIDTH,P_STROKE_H,P_STROKE_S,P_STROKE_B,P_STROKE_A,
  P_SHADOW,P_SHADOW_INTENSITY,P_SHADOW_BLUR,P_SHADOW_X,P_SHADOW_Y,P_SHADOW_H,P_SHADOW_S,P_SHADOW_B,P_SHADOW_A,
  P_GLOW,P_GLOW_INTENSITY,P_GLOW_SIZE,P_GLOW_H,P_GLOW_S,P_GLOW_B,P_GLOW_A,
  P_BOX_MODE,P_BOX_WIDTH,P_BOX_HEIGHT,P_PAD_X,P_PAD_Y,P_POS_X,P_POS_Y,P_RADIUS,
  P_BOX_H,P_BOX_S,P_BOX_B,P_BOX_A,P_BOX_STROKE_WIDTH,P_BOX_STROKE_H,P_BOX_STROKE_S,P_BOX_STROKE_B,P_BOX_STROKE_A,
  P_CREDIT,P_COUNT
};

struct SharedState{
  std::mutex mutex;
  std::string raw="Malam kudus sunyi senyap dunia terlelap\nHanya dua berjaga terus ayah bunda yang tinggal\nDi kandang domba menjaga Anak yang Maha Kudus";
  std::vector<std::string> pngFiles;
  std::vector<std::string> cues;
  std::vector<float> values;
  std::string font="Arial";
  size_t current=0;
  bool preview=true;
  unsigned long long cueRevision=1,styleRevision=1;
};
SharedState g;
std::once_flag initFlag;
std::wstring autoSavePath();
std::wstring pngListPath();
bool readUtf8(const std::wstring&,std::string&);

static CFFGLPluginInfo PluginInfo(PluginFactory<BRLirik>,"BRLK","BR Lirik",3,0,0,300,FF_SOURCE,"Text and PNG live lyrics source by BELAJAR RESOLUME","Effect by Belajar Resolume");

float c01(float x){return std::max(0.f,std::min(1.f,x));}
std::wstring u8w(const std::string& s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);if(n<=0)return std::wstring(s.begin(),s.end());std::wstring r(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,r.data(),n);r.pop_back();return r;}
std::string w8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,nullptr,0,nullptr,nullptr);std::string r(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,r.data(),n,nullptr,nullptr);r.pop_back();return r;}
std::vector<std::string> splitCues(std::string text){std::vector<std::string> out;std::string line;std::stringstream ss(text);while(std::getline(ss,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();out.push_back(line);}if(out.empty())out.push_back("");return out;}
std::vector<std::string> buildCues(const std::string&text,bool blankEdges){auto out=splitCues(text);if(blankEdges){if(out.empty()||!out.front().empty())out.insert(out.begin(),"");if(out.empty()||!out.back().empty())out.push_back("");}return out;}
std::vector<std::string> buildPngCues(const std::vector<std::string>&files,bool blankEdges){auto out=files;if(out.empty())out.push_back("");if(blankEdges){if(!out.front().empty())out.insert(out.begin(),"");if(!out.back().empty())out.push_back("");}return out;}
void rebuildActiveCuesLocked(){bool png=g.values[P_INPUT_MODE]>.5f;bool blank=g.values[P_BLANK_EDGES]>.5f;g.cues=png?buildPngCues(g.pngFiles,blank):buildCues(g.raw,blank);g.current=0;g.cueRevision++;}
std::string trimCopy(std::string s){auto space=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};while(!s.empty()&&space((unsigned char)s.front()))s.erase(s.begin());while(!s.empty()&&space((unsigned char)s.back()))s.pop_back();return s;}
std::string removeSectionLabels(const std::string&text){std::stringstream in(text);std::string line,out;bool first=true;while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();std::string t=trimCopy(line);bool label=t.size()>=2&&t.front()=='['&&t.back()==']';if(label)continue;if(!first)out+='\n';out+=line;first=false;}return out;}
void initShared(){
  std::string saved;if(readUtf8(autoSavePath(),saved)&&!saved.empty())g.raw=saved;
  std::string savedPng;if(readUtf8(pngListPath(),savedPng)){std::stringstream ss(savedPng);std::string line;while(std::getline(ss,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty())g.pngFiles.push_back(line);}}
  g.values.assign(P_COUNT,0.f);
  auto&v=g.values;v[P_INPUT_MODE]=0;v[P_PNG_FIT]=0;v[P_PNG_SCALE]=.5f;v[P_SHOW_PREVIEW]=1;v[P_BLANK_EDGES]=1;v[P_TRANSITION]=1;v[P_TRANSITION_DURATION]=.18f;v[P_TRANSITION_SMOOTH]=.8f;v[P_TRANSITION_LOCK]=1;v[P_STACK_SCALE]=.32f;v[P_STACK_OPACITY]=.42f;v[P_STACK_GAP]=.18f;
  v[P_MOTION]=0;v[P_MOTION_AMOUNT]=.18f;v[P_MOTION_SPEED]=.18f;v[P_FONT_SIZE]=.24f;v[P_AUTO_FIT]=1;v[P_ALIGN]=1;v[P_VALIGN]=1;v[P_LINE_SPACING]=.5f;
  v[P_TEXT_B]=1;v[P_TEXT_A]=1;v[P_TEXT_FILL]=1;v[P_GRADIENT_DIRECTION]=1;v[P_GRADIENT_H]=.58f;v[P_GRADIENT_S]=.65f;v[P_GRADIENT_B]=1;v[P_GRADIENT_A]=1;v[P_TEXT_STROKE]=1;v[P_TEXT_STROKE_WIDTH]=.10f;v[P_STROKE_B]=0;v[P_STROKE_A]=1;
  v[P_SHADOW]=1;v[P_SHADOW_INTENSITY]=.45f;v[P_SHADOW_BLUR]=.18f;v[P_SHADOW_X]=.53f;v[P_SHADOW_Y]=.55f;v[P_SHADOW_B]=0;v[P_SHADOW_A]=.8f;
  v[P_GLOW_INTENSITY]=.3f;v[P_GLOW_SIZE]=.18f;v[P_GLOW_B]=1;v[P_GLOW_A]=.75f;
  v[P_BOX_MODE]=0;v[P_BOX_WIDTH]=.72f;v[P_BOX_HEIGHT]=.24f;v[P_PAD_X]=.08f;v[P_PAD_Y]=.12f;v[P_POS_X]=.5f;v[P_POS_Y]=.78f;v[P_RADIUS]=.18f;
  v[P_BOX_B]=.05f;v[P_BOX_A]=.72f;v[P_BOX_STROKE_WIDTH]=.08f;v[P_BOX_STROKE_B]=1;v[P_BOX_STROKE_A]=1;
  g.cues=buildCues(g.raw,true);
}

int CALLBACK enumFont(const LOGFONTW*lf,const TEXTMETRICW*,DWORD,LPARAM p){auto*s=reinterpret_cast<std::set<std::wstring>*>(p);if(lf->lfFaceName[0]!=L'@')s->insert(lf->lfFaceName);return 1;}
Color hsva(const std::vector<float>&v,int h,int s,int b,int a){float rgb[3];HSVtoRGB(v[h],v[s],v[b],rgb[0],rgb[1],rgb[2]);return Color(BYTE(c01(v[a])*255),BYTE(c01(rgb[0])*255),BYTE(c01(rgb[1])*255),BYTE(c01(rgb[2])*255));}
void rounded(GraphicsPath&path,const RectF&r,float rad){float d=std::min(rad*2.f,std::min(r.Width,r.Height));if(d<=1){path.AddRectangle(r);return;}path.AddArc(r.X,r.Y,d,d,180,90);path.AddArc(r.GetRight()-d,r.Y,d,d,270,90);path.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);path.AddArc(r.X,r.GetBottom()-d,d,d,90,90);path.CloseFigure();}
std::wstring autoSavePath(){wchar_t base[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);std::wstring root=n?base:L".";root+=L"\\Belajar Resolume";CreateDirectoryW(root.c_str(),nullptr);root+=L"\\BR Lirik";CreateDirectoryW(root.c_str(),nullptr);return root+L"\\autosave.txt";}
std::wstring pngListPath(){auto path=autoSavePath();auto slash=path.find_last_of(L"\\/");return path.substr(0,slash+1)+L"png-cues.txt";}
bool writeUtf8(const std::wstring&path,const std::string&data){std::ofstream f(std::filesystem::path(path),std::ios::binary);if(!f)return false;f.write(data.data(),std::streamsize(data.size()));return bool(f);}
bool readUtf8(const std::wstring&path,std::string&data){std::ifstream f(std::filesystem::path(path),std::ios::binary);if(!f)return false;data.assign(std::istreambuf_iterator<char>(f),{});if(data.size()>=3&&(unsigned char)data[0]==0xEF&&(unsigned char)data[1]==0xBB&&(unsigned char)data[2]==0xBF)data.erase(0,3);return true;}
std::wstring chooseFile(HWND owner,bool save){wchar_t file[MAX_PATH]=L"BR-Lirik.txt";OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=owner;o.lpstrFilter=L"Text File (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";o.lpstrFile=file;o.nMaxFile=MAX_PATH;o.lpstrDefExt=L"txt";o.Flags=OFN_EXPLORER|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);BOOL ok=save?GetSaveFileNameW(&o):GetOpenFileNameW(&o);return ok?std::wstring(file):L"";}
std::vector<std::wstring> choosePngFiles(){
  std::vector<wchar_t> buffer(65536,L'\0');OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.lpstrFilter=L"PNG Image (*.png)\0*.png\0";o.lpstrFile=buffer.data();o.nMaxFile=DWORD(buffer.size());o.lpstrTitle=L"Pilih PNG Lirik (bisa pilih banyak)";o.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_ALLOWMULTISELECT;
  if(!GetOpenFileNameW(&o))return {};
  std::vector<std::wstring> out;std::wstring first=buffer.data();const wchar_t*p=buffer.data()+first.size()+1;
  if(*p==L'\0'){out.push_back(first);return out;}
  std::filesystem::path dir(first);while(*p){std::wstring name=p;out.push_back((dir/name).wstring());p+=name.size()+1;}return out;
}

const char* vertexShader=R"GLSL(#version 410 core
layout(location=0)in vec4 vPosition;layout(location=1)in vec2 vUV;out vec2 uv;void main(){gl_Position=vPosition;uv=vUV;})GLSL";
const char* fragmentShader=R"GLSL(#version 410 core
uniform sampler2D currentTex,previousTex,nextTex;uniform float progress,transitionMode,smoothness,motionMode,motionAmount,motionSpeed,elapsed,stackScale,stackOpacity,stackGap,outputRole;uniform vec2 resolution,anchor;in vec2 uv;out vec4 fragColor;
vec4 rawSample(sampler2D t,vec2 p){if(any(lessThan(p,vec2(0)))||any(greaterThan(p,vec2(1))))return vec4(0);return texture(t,vec2(p.x,1.-p.y));}
vec4 transformed(sampler2D t,vec2 p,vec2 off,float scale){vec2 q=(p-anchor-off)/max(scale,.01)+anchor;return rawSample(t,q);}
vec4 blurred(sampler2D t,vec2 p,float amount){vec2 d=vec2(amount/resolution.x,0);return rawSample(t,p-d*2.)*.1+rawSample(t,p-d)*.2+rawSample(t,p)*.4+rawSample(t,p+d)*.2+rawSample(t,p+d*2.)*.1;}
vec4 over(vec4 top,vec4 bottom){return top+bottom*(1.-top.a);}
void main(){float linear=clamp(progress,0.,1.),p=mix(linear,linear*linear*(3.-2.*linear),smoothness);vec2 drift=vec2(0);if(motionMode>.5&&motionMode<1.5)drift=vec2(sin(elapsed*motionSpeed),cos(elapsed*motionSpeed*.73))*motionAmount/resolution;if(motionMode>1.5)drift=vec2(sin(elapsed*motionSpeed*17.),sin(elapsed*motionSpeed*23.7))*motionAmount/resolution;
 if(transitionMode>13.5&&outputRole<.5){float ss=mix(.58,.92,stackScale),gap=mix(.04,.30,stackGap);vec4 oldC=transformed(previousTex,uv,drift+vec2(0,p*.28),1.)*(1.-p);vec4 currentC=transformed(currentTex,uv,drift+vec2(0,-gap*(1.-p)),mix(ss,1.,p));vec4 nextC=transformed(nextTex,uv,drift+vec2(0,-gap),ss)*(stackOpacity*p);fragColor=over(currentC,over(nextC,oldC));return;}
 vec2 op=drift,np=drift;float oa=1.,na=1.,os=1.,ns=1.;if(transitionMode<.5){oa=0.;na=1.;}
 else if(transitionMode<1.5){oa=1.-p;na=p;}
 else if(transitionMode<2.5){oa=1.-smoothstep(0.,.5,p);na=smoothstep(.5,1.,p);}
 else if(transitionMode<3.5){op+=vec2(-p,0);np+=vec2(1.-p,0);oa=1.-p;na=p;}
 else if(transitionMode<4.5){op+=vec2(p,0);np+=vec2(p-1.,0);oa=1.-p;na=p;}
 else if(transitionMode<5.5){op+=vec2(0,p);np+=vec2(0,p-1.);oa=1.-p;na=p;}
 else if(transitionMode<6.5){op+=vec2(0,-p);np+=vec2(0,1.-p);oa=1.-p;na=p;}
 else if(transitionMode<7.5){os=mix(1.,1.25,p);ns=mix(.55,1.,p);oa=1.-p;na=p;}
 else if(transitionMode<8.5){os=mix(1.,.65,p);ns=mix(1.45,1.,p);oa=1.-p;na=p;}
 else if(transitionMode<9.5){float bounce=1.+sin(p*3.14159265)*.13*(1.-p);ns=mix(.45,1.,p)*bounce;oa=1.-p;na=p;}
 vec4 oldC,newC;if(transitionMode>9.5&&transitionMode<10.5){oldC=rawSample(previousTex,uv-drift)*(1.-smoothstep(0.,1.,p+uv.x));newC=rawSample(currentTex,uv-drift)*smoothstep(0.,1.,p*1.5-uv.x+.25);}
 else if(transitionMode>10.5&&transitionMode<11.5){oldC=rawSample(previousTex,uv-drift)*(1.-smoothstep(0.,1.,p+(1.-uv.x)));newC=rawSample(currentTex,uv-drift)*smoothstep(0.,1.,p*1.5-(1.-uv.x)+.25);}
 else if(transitionMode>11.5&&transitionMode<12.5){oldC=rawSample(previousTex,uv-drift)*(1.-smoothstep(0.,1.,p+uv.y));newC=rawSample(currentTex,uv-drift)*smoothstep(0.,1.,p*1.5-uv.y+.25);}
 else if(transitionMode>12.5&&transitionMode<13.5){oldC=blurred(previousTex,uv-drift,p*18.)*(1.-p);newC=blurred(currentTex,uv-drift,(1.-p)*18.)*p;}
 else{oldC=transformed(previousTex,uv,op,os)*oa;newC=transformed(currentTex,uv,np,ns)*na;}fragColor=over(newC,oldC);})GLSL";
}

BRLirik::BRLirik(){
  std::call_once(initFlag,initShared);SetMinInputs(0);SetMaxInputs(0);GdiplusStartupInput si;GdiplusStartup(&gdiplusToken_,&si,nullptr);enumerateFonts();
  SetOptionParamInfo(P_ROLE,"Output Role",2,0);addOption(P_ROLE,0,"PROGRAM",0);addOption(P_ROLE,1,"PREVIEW - NEXT",1);
  SetOptionParamInfo(P_INPUT_MODE,"Input Mode",2,0);addOption(P_INPUT_MODE,0,"TEXT LYRICS",0);addOption(P_INPUT_MODE,1,"PNG LYRICS",1);SetParamInfof(P_EDIT,"Add / Edit Text Lyrics",FF_TYPE_EVENT);SetParamInfof(P_BROWSE_PNG,"Browse PNG Lyrics",FF_TYPE_EVENT);SetOptionParamInfo(P_PNG_FIT,"PNG Fit Mode",3,0);addOption(P_PNG_FIT,0,"Fit / Contain",0);addOption(P_PNG_FIT,1,"Fill / Cover",1);addOption(P_PNG_FIT,2,"Stretch",2);SetParamInfof(P_PNG_SCALE,"PNG Scale",FF_TYPE_STANDARD);
  SetParamInfof(P_PREVIOUS,"Previous",FF_TYPE_EVENT);SetParamInfof(P_NEXT,"Next",FF_TYPE_EVENT);SetParamInfof(P_GOTO,"Go To",FF_TYPE_EVENT);SetParamInfo(P_GOTO_NUMBER,"Go To Cue Number",FF_TYPE_TEXT,gotoText_.c_str());SetParamInfof(P_RESET,"Reset to First Blank",FF_TYPE_EVENT);SetParamInfof(P_SHOW_PREVIEW,"Show Preview Output",FF_TYPE_BOOLEAN);SetParamInfof(P_BLANK_EDGES,"Blank First & End Lyrics",FF_TYPE_BOOLEAN);SetParamInfo(P_STATUS,"Cue Status",FF_TYPE_TEXT,"Cue 1 / 1");
  SetOptionParamInfo(P_TRANSITION,"Transition",15,1);const char*t[]={"Cut To","Cross Fade","Fade Through Transparent","Slide Left + Fade","Slide Right + Fade","Roll Up + Fade","Roll Down + Fade","Zoom In + Fade","Zoom Out + Fade","Pop / Bounce In","Wipe Left","Wipe Right","Wipe Up","Blur + Fade","Stacked Next Preview"};for(unsigned i=0;i<15;i++)addOption(P_TRANSITION,i,t[i],float(i));SetParamInfof(P_TRANSITION_DURATION,"Transition Duration",FF_TYPE_STANDARD);SetParamInfof(P_TRANSITION_SMOOTH,"Transition Smoothness",FF_TYPE_STANDARD);SetParamInfof(P_TRANSITION_LOCK,"Lock Next During Animation",FF_TYPE_BOOLEAN);SetParamInfof(P_STACK_SCALE,"Stack Next Scale",FF_TYPE_STANDARD);SetParamInfof(P_STACK_OPACITY,"Stack Next Opacity",FF_TYPE_STANDARD);SetParamInfof(P_STACK_GAP,"Stack Next Gap",FF_TYPE_STANDARD);
  SetOptionParamInfo(P_MOTION,"Live Motion",3,0);addOption(P_MOTION,0,"None",0);addOption(P_MOTION,1,"Slow Floating",1);addOption(P_MOTION,2,"Rock Shake",2);SetParamInfof(P_MOTION_AMOUNT,"Motion Amount",FF_TYPE_STANDARD);SetParamInfof(P_MOTION_SPEED,"Motion Speed",FF_TYPE_STANDARD);
  SetParamInfo(P_FONT_NAME,"Font Search / Name",FF_TYPE_TEXT,fontName_.c_str());SetOptionParamInfo(P_FONT_LIST,"Installed Font",unsigned(std::max<size_t>(fonts_.size(),1)),0);if(fonts_.empty())addOption(P_FONT_LIST,0,"Arial",0);else for(unsigned i=0;i<fonts_.size();i++)addOption(P_FONT_LIST,i,fonts_[i].c_str(),float(i));SetParamInfof(P_REFRESH_FONTS,"Refresh Fonts",FF_TYPE_EVENT);SetParamInfof(P_FONT_SIZE,"Font Size",FF_TYPE_STANDARD);SetParamInfof(P_AUTO_FIT,"Auto Fit Height",FF_TYPE_BOOLEAN);SetOptionParamInfo(P_ALIGN,"Horizontal Align",3,1);addOption(P_ALIGN,0,"Left",0);addOption(P_ALIGN,1,"Center",1);addOption(P_ALIGN,2,"Right",2);SetOptionParamInfo(P_VALIGN,"Vertical Align",3,1);addOption(P_VALIGN,0,"Top",0);addOption(P_VALIGN,1,"Middle",1);addOption(P_VALIGN,2,"Bottom",2);SetParamInfof(P_LINE_SPACING,"Line Spacing",FF_TYPE_STANDARD);SetOptionParamInfo(P_POSITION_PRESET,"Screen Position",4,0);addOption(P_POSITION_PRESET,0,"Custom",0);addOption(P_POSITION_PRESET,1,"Fit to Top",1);addOption(P_POSITION_PRESET,2,"Fit to Middle",2);addOption(P_POSITION_PRESET,3,"Fit to Bottom",3);
  SetParamInfof(P_TEXT_H,"Text Color A",FF_TYPE_HUE);SetParamInfof(P_TEXT_S,"Text A Saturation",FF_TYPE_SATURATION);SetParamInfof(P_TEXT_B,"Text A Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_TEXT_A,"Text A Opacity",FF_TYPE_ALPHA);SetParamInfof(P_TEXT_FILL,"Text Fill",FF_TYPE_BOOLEAN);SetParamInfof(P_GRADIENT,"Font Gradient",FF_TYPE_BOOLEAN);SetOptionParamInfo(P_GRADIENT_DIRECTION,"Gradient Direction",4,1);addOption(P_GRADIENT_DIRECTION,0,"Left to Right",0);addOption(P_GRADIENT_DIRECTION,1,"Top to Bottom",1);addOption(P_GRADIENT_DIRECTION,2,"Diagonal Down",2);addOption(P_GRADIENT_DIRECTION,3,"Diagonal Up",3);SetParamInfof(P_GRADIENT_H,"Gradient Color B",FF_TYPE_HUE);SetParamInfof(P_GRADIENT_S,"Gradient B Saturation",FF_TYPE_SATURATION);SetParamInfof(P_GRADIENT_B,"Gradient B Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_GRADIENT_A,"Gradient B Opacity",FF_TYPE_ALPHA);SetParamInfof(P_TEXT_STROKE,"Text Stroke",FF_TYPE_BOOLEAN);SetParamInfof(P_TEXT_STROKE_WIDTH,"Stroke Width",FF_TYPE_STANDARD);SetParamInfof(P_STROKE_H,"Stroke Color",FF_TYPE_HUE);SetParamInfof(P_STROKE_S,"Stroke Saturation",FF_TYPE_SATURATION);SetParamInfof(P_STROKE_B,"Stroke Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_STROKE_A,"Stroke Opacity",FF_TYPE_ALPHA);
  SetParamInfof(P_SHADOW,"Drop Shadow",FF_TYPE_BOOLEAN);SetParamInfof(P_SHADOW_INTENSITY,"Shadow Intensity",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_BLUR,"Shadow Softness",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_X,"Shadow Offset X",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_Y,"Shadow Offset Y",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_H,"Shadow Color",FF_TYPE_HUE);SetParamInfof(P_SHADOW_S,"Shadow Saturation",FF_TYPE_SATURATION);SetParamInfof(P_SHADOW_B,"Shadow Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_SHADOW_A,"Shadow Opacity",FF_TYPE_ALPHA);
  SetParamInfof(P_GLOW,"Glow",FF_TYPE_BOOLEAN);SetParamInfof(P_GLOW_INTENSITY,"Glow Intensity",FF_TYPE_STANDARD);SetParamInfof(P_GLOW_SIZE,"Glow Size",FF_TYPE_STANDARD);SetParamInfof(P_GLOW_H,"Glow Color",FF_TYPE_HUE);SetParamInfof(P_GLOW_S,"Glow Saturation",FF_TYPE_SATURATION);SetParamInfof(P_GLOW_B,"Glow Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_GLOW_A,"Glow Opacity",FF_TYPE_ALPHA);
  SetOptionParamInfo(P_BOX_MODE,"Text Box",4,0);addOption(P_BOX_MODE,0,"Off",0);addOption(P_BOX_MODE,1,"Fill",1);addOption(P_BOX_MODE,2,"Fill + Stroke",2);addOption(P_BOX_MODE,3,"Stroke Only",3);SetParamInfof(P_BOX_A,"Box Opacity",FF_TYPE_ALPHA);SetParamInfof(P_BOX_WIDTH,"Box Width",FF_TYPE_STANDARD);SetParamInfof(P_BOX_HEIGHT,"Box Height",FF_TYPE_STANDARD);SetParamInfof(P_PAD_X,"Horizontal Padding",FF_TYPE_STANDARD);SetParamInfof(P_PAD_Y,"Vertical Padding",FF_TYPE_STANDARD);SetParamInfof(P_POS_X,"Position X",FF_TYPE_XPOS);SetParamInfof(P_POS_Y,"Position Y",FF_TYPE_YPOS);SetParamInfof(P_RADIUS,"Round Box",FF_TYPE_STANDARD);SetParamInfof(P_BOX_H,"Box Color",FF_TYPE_HUE);SetParamInfof(P_BOX_S,"Box Saturation",FF_TYPE_SATURATION);SetParamInfof(P_BOX_B,"Box Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_BOX_STROKE_WIDTH,"Box Stroke Width",FF_TYPE_STANDARD);SetParamInfof(P_BOX_STROKE_H,"Box Stroke Color",FF_TYPE_HUE);SetParamInfof(P_BOX_STROKE_S,"Box Stroke Saturation",FF_TYPE_SATURATION);SetParamInfof(P_BOX_STROKE_B,"Box Stroke Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_BOX_STROKE_A,"Box Stroke Opacity",FF_TYPE_ALPHA);SetParamInfo(P_CREDIT,"Effect by Belajar Resolume",FF_TYPE_TEXT,credit_.c_str());
  for(unsigned i=P_ROLE;i<=P_PNG_SCALE;i++)SetParamGroup(i,"Input Source");for(unsigned i=P_PREVIOUS;i<=P_STATUS;i++)SetParamGroup(i,"Live Control");for(unsigned i=P_TRANSITION;i<=P_STACK_GAP;i++)SetParamGroup(i,"Transition");for(unsigned i=P_MOTION;i<=P_MOTION_SPEED;i++)SetParamGroup(i,"Live Motion");for(unsigned i=P_FONT_NAME;i<=P_POSITION_PRESET;i++)SetParamGroup(i,"Typography");for(unsigned i=P_TEXT_H;i<=P_STROKE_A;i++)SetParamGroup(i,"Text Style");for(unsigned i=P_SHADOW;i<=P_SHADOW_A;i++)SetParamGroup(i,"Drop Shadow");for(unsigned i=P_GLOW;i<=P_GLOW_A;i++)SetParamGroup(i,"Glow");for(unsigned i=P_BOX_MODE;i<=P_BOX_STROKE_A;i++)SetParamGroup(i,"Text / PNG Box");SetParamGroup(P_CREDIT,"Credit");lastFrame_=std::chrono::steady_clock::now();
}

BRLirik::~BRLirik(){editorStopRequested_=true;if(HWND w=editorWindow_.load())PostMessageW(w,WM_CLOSE,0,0);if(editorThread_.joinable())editorThread_.join();releaseTextures();if(gdiplusToken_)GdiplusShutdown(gdiplusToken_);}
void BRLirik::addOption(unsigned p,unsigned e,const char*n,float v){SetParamElementInfo(p,e,n,v);}
void BRLirik::enumerateFonts(){std::set<std::wstring>names;HDC dc=GetDC(nullptr);LOGFONTW lf{};lf.lfCharSet=DEFAULT_CHARSET;EnumFontFamiliesExW(dc,&lf,reinterpret_cast<FONTENUMPROCW>(enumFont),reinterpret_cast<LPARAM>(&names),0);ReleaseDC(nullptr,dc);fonts_.clear();for(auto&x:names)fonts_.push_back(w8(x));}

void BRLirik::openEditor(){if(editorRunning_){if(HWND w=editorWindow_.load())PostMessageW(w,WM_APP+1,0,0);return;}if(editorThread_.joinable())editorThread_.join();editorStopRequested_=false;editorRunning_=true;editorThread_=std::thread(&BRLirik::runEditor,this);}
void BRLirik::runEditor(){static const wchar_t*cn=L"BRLirikEditorWindow";HINSTANCE hi=GetModuleHandleW(nullptr);WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=EditorWndProc;wc.hInstance=hi;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=cn;if(RegisterClassExW(&wc)==0&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){editorRunning_=false;return;}HWND w=CreateWindowExW(WS_EX_TOOLWINDOW,cn,L"BR Lirik — Lyrics Editor",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,900,680,nullptr,nullptr,hi,this);if(!w){editorRunning_=false;return;}editorWindow_=w;MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}editorWindow_=nullptr;editorRunning_=false;}

LRESULT CALLBACK BRLirik::EditorWndProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
  auto*self=reinterpret_cast<BRLirik*>(GetWindowLongPtrW(w,GWLP_USERDATA));
  if(msg==WM_NCCREATE){auto*c=reinterpret_cast<CREATESTRUCTW*>(lp);self=reinterpret_cast<BRLirik*>(c->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(!self)return DefWindowProcW(w,msg,wp,lp);
  constexpr int TXT=1001,UPDATE=1002,LOAD=1003,SAVE=1004,CLEAR=1005,CANCEL=1006,CLEAN_CHECK=1009,CLEAN_NOW=1010;
  auto menu=[](int id){return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));};
  switch(msg){
    case WM_CREATE:{
      HFONT ui=reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      CreateWindowExW(0,L"STATIC",L"Satu baris input = satu cue. Baris kosong = clear output.",WS_CHILD|WS_VISIBLE,16,12,820,20,w,nullptr,nullptr,nullptr);
      HWND cleanCheck=CreateWindowExW(0,L"BUTTON",L"Hilangkan baris [CHORUS], [VERSE], [BRIDGE], dan label [LAINNYA]",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,16,38,520,24,w,menu(CLEAN_CHECK),nullptr,nullptr);
      HWND cleanNow=CreateWindowExW(0,L"BUTTON",L"Clean Preview",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,708,35,118,28,w,menu(CLEAN_NOW),nullptr,nullptr);
      self->editorTextBox_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_LEFT|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,16,72,810,472,w,menu(TXT),nullptr,nullptr);
      const wchar_t*labels[]={L"Load TXT",L"Save TXT",L"Clear",L"Cancel",L"Update Lyrics"};int ids[]={LOAD,SAVE,CLEAR,CANCEL,UPDATE};int x=16;
      for(int i=0;i<5;i++){int bw=i==4?142:112;HWND b=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|(i==4?BS_DEFPUSHBUTTON:BS_PUSHBUTTON),x,558,bw,34,w,menu(ids[i]),nullptr,nullptr);SendMessageW(b,WM_SETFONT,reinterpret_cast<WPARAM>(ui),TRUE);x+=bw+12;}
      SendMessageW(cleanCheck,WM_SETFONT,reinterpret_cast<WPARAM>(ui),TRUE);SendMessageW(cleanNow,WM_SETFONT,reinterpret_cast<WPARAM>(ui),TRUE);SendMessageW(self->editorTextBox_,WM_SETFONT,reinterpret_cast<WPARAM>(ui),TRUE);
      std::string raw;{std::lock_guard<std::mutex>lk(g.mutex);raw=g.raw;}auto wide=u8w(raw);SetWindowTextW(self->editorTextBox_,wide.c_str());SetFocus(self->editorTextBox_);return 0;
    }
    case WM_SIZE:{
      int W=LOWORD(lp),H=HIWORD(lp);HWND cleanNow=GetDlgItem(w,CLEAN_NOW);if(cleanNow)MoveWindow(cleanNow,std::max(310,W-134),35,118,28,TRUE);
      if(self->editorTextBox_)MoveWindow(self->editorTextBox_,16,72,std::max(240,W-32),std::max(80,H-134),TRUE);int y=std::max(78,H-50),x=16;for(int id:{LOAD,SAVE,CLEAR,CANCEL,UPDATE}){HWND b=GetDlgItem(w,id);int bw=id==UPDATE?142:112;if(b)MoveWindow(b,x,y,bw,34,TRUE);x+=bw+12;}return 0;
    }
    case WM_COMMAND:{
      int id=LOWORD(wp);auto textFrom=[&](HWND edit){int n=GetWindowTextLengthW(edit);std::wstring s(size_t(n)+1,L'\0');GetWindowTextW(edit,s.data(),n+1);s.resize(size_t(n));return w8(s);};
      auto setLyrics=[&](const std::string&data){auto wide=u8w(data);SetWindowTextW(self->editorTextBox_,wide.c_str());};
      if(id==CLEAN_NOW){setLyrics(removeSectionLabels(textFrom(self->editorTextBox_)));return 0;}
      if(id==LOAD){auto p=chooseFile(w,false);std::string data;if(!p.empty()&&readUtf8(p,data))setLyrics(data);return 0;}
      if(id==SAVE){auto p=chooseFile(w,true);if(!p.empty())writeUtf8(p,textFrom(self->editorTextBox_));return 0;}
      if(id==CLEAR){SetWindowTextW(self->editorTextBox_,L"");return 0;}
      if(id==UPDATE){std::string raw=textFrom(self->editorTextBox_);if(SendMessageW(GetDlgItem(w,CLEAN_CHECK),BM_GETCHECK,0,0)==BST_CHECKED)raw=removeSectionLabels(raw);{std::lock_guard<std::mutex>lk(g.mutex);g.raw=raw;g.values[P_INPUT_MODE]=0;rebuildActiveCuesLocked();}writeUtf8(autoSavePath(),raw);DestroyWindow(w);return 0;}
      if(id==CANCEL){DestroyWindow(w);return 0;}break;
    }
    case WM_APP+1:ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);SetFocus(self->editorTextBox_);return 0;
    case WM_CLOSE:DestroyWindow(w);return 0;
    case WM_NCDESTROY:self->editorTextBox_=nullptr;self->editorWindow_=nullptr;SetWindowLongPtrW(w,GWLP_USERDATA,0);PostQuitMessage(0);break;
  }
  return DefWindowProcW(w,msg,wp,lp);
}

std::string BRLirik::cueForRole(bool previous)const{std::lock_guard<std::mutex>lk(g.mutex);if(g.cues.empty())return "";size_t index=g.current;if(role_>.5f){if(!g.preview||g.current+1>=g.cues.size())return "";index=g.current+1;}if(previous&&index>0)index--;return g.cues[index];}

unsigned int BRLirik::buildFrameTexture(const std::string&text,bool preview,unsigned int&tw,unsigned int&th){
  std::vector<float>v;std::string font;size_t current=0,total=1;{std::lock_guard<std::mutex>lk(g.mutex);v=g.values;font=g.font;current=g.current;total=g.cues.size();}
  tw=std::max(2u,viewportWidth_);th=std::max(2u,viewportHeight_);Bitmap bmp(tw,th,PixelFormat32bppPARGB);Graphics gr(&bmp);gr.SetSmoothingMode(SmoothingModeAntiAlias);gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);gr.Clear(Color(0,0,0,0));
  float bw=std::max(120.f,v[P_BOX_WIDTH]*tw),bh=std::max(60.f,v[P_BOX_HEIGHT]*th),cx=v[P_POS_X]*tw,cy=v[P_POS_Y]*th;RectF box(cx-bw*.5f,cy-bh*.5f,bw,bh);float radius=v[P_RADIUS]*std::min(bw,bh)*.5f;GraphicsPath boxPath;rounded(boxPath,box,radius);int bm=int(v[P_BOX_MODE]+.5f);if(bm==1||bm==2){SolidBrush bb(hsva(v,P_BOX_H,P_BOX_S,P_BOX_B,P_BOX_A));gr.FillPath(&bb,&boxPath);}if(bm==2||bm==3){Pen bp(hsva(v,P_BOX_STROKE_H,P_BOX_STROKE_S,P_BOX_STROKE_B,P_BOX_STROKE_A),1.f+v[P_BOX_STROKE_WIDTH]*24.f);bp.SetLineJoin(LineJoinRound);gr.DrawPath(&bp,&boxPath);}
  float px=8.f+v[P_PAD_X]*bw*.35f,py=5.f+v[P_PAD_Y]*bh*.35f;RectF area(box.X+px,box.Y+py,std::max(20.f,box.Width-2*px),std::max(20.f,box.Height-2*py));
  bool pngMode=v[P_INPUT_MODE]>.5f;
  if(pngMode&&!text.empty()){
    std::wstring path=u8w(text);Image image(path.c_str());
    if(image.GetLastStatus()==Ok&&image.GetWidth()>0&&image.GetHeight()>0){
      gr.SetInterpolationMode(InterpolationModeHighQualityBicubic);GraphicsState imageState=gr.Save();gr.SetClip(area);
      float iw=float(image.GetWidth()),ih=float(image.GetHeight()),baseW=area.Width,baseH=area.Height;int fit=int(v[P_PNG_FIT]+.5f);
      if(fit<2){float scale=fit==0?std::min(area.Width/iw,area.Height/ih):std::max(area.Width/iw,area.Height/ih);baseW=iw*scale;baseH=ih*scale;}
      float userScale=.35f+v[P_PNG_SCALE]*1.3f;float dw=baseW*userScale,dh=baseH*userScale;RectF dest(area.X+(area.Width-dw)*.5f,area.Y+(area.Height-dh)*.5f,dw,dh);
      auto tinted=[&](const RectF&d,const Color&c,float alpha){
        ColorMatrix matrix={};matrix.m[3][0]=c.GetR()/255.f;matrix.m[3][1]=c.GetG()/255.f;matrix.m[3][2]=c.GetB()/255.f;matrix.m[3][3]=c.GetA()/255.f*alpha;matrix.m[4][4]=1.f;
        ImageAttributes attrs;attrs.SetColorMatrix(&matrix,ColorMatrixFlagsDefault,ColorAdjustTypeBitmap);gr.DrawImage(&image,d,0,0,iw,ih,UnitPixel,&attrs);
      };
      if(v[P_GLOW]>.5f){Color gc=hsva(v,P_GLOW_H,P_GLOW_S,P_GLOW_B,P_GLOW_A);float r=2.f+v[P_GLOW_SIZE]*28.f;for(int ring=3;ring>=1;--ring){float rr=r*ring/3.f,a=v[P_GLOW_INTENSITY]/(ring*2.f);for(int k=0;k<8;k++){float q=float(k)*3.14159265f/4.f;RectF d(dest.X+cosf(q)*rr,dest.Y+sinf(q)*rr,dest.Width,dest.Height);tinted(d,gc,a);}}}
      if(v[P_SHADOW]>.5f){Color sc=hsva(v,P_SHADOW_H,P_SHADOW_S,P_SHADOW_B,P_SHADOW_A);float dx=(v[P_SHADOW_X]-.5f)*80.f,dy=(v[P_SHADOW_Y]-.5f)*80.f,soft=1.f+v[P_SHADOW_BLUR]*18.f;for(int i=3;i>=1;--i){RectF d(dest.X+dx+soft*(i-2),dest.Y+dy+soft*(i-2),dest.Width,dest.Height);tinted(d,sc,v[P_SHADOW_INTENSITY]/i);}}
      gr.DrawImage(&image,dest,0,0,iw,ih,UnitPixel);
      gr.Restore(imageState);
    }
  }else if(!pngMode){
    std::wstring content=u8w(text.empty()?" ":text),familyW=u8w(font);FontFamily selected(familyW.c_str()),fallback(L"Arial");FontFamily*family=selected.IsAvailable()?&selected:&fallback;StringFormat fmt(StringFormat::GenericTypographic());fmt.SetTrimming(StringTrimmingWord);fmt.SetFormatFlags(0);if(v[P_ALIGN]<.5f)fmt.SetAlignment(StringAlignmentNear);else if(v[P_ALIGN]<1.5f)fmt.SetAlignment(StringAlignmentCenter);else fmt.SetAlignment(StringAlignmentFar);if(v[P_VALIGN]<.5f)fmt.SetLineAlignment(StringAlignmentNear);else if(v[P_VALIGN]<1.5f)fmt.SetLineAlignment(StringAlignmentCenter);else fmt.SetLineAlignment(StringAlignmentFar);
    float fs=18.f+v[P_FONT_SIZE]*190.f;RectF measured,measureLayout(0.f,0.f,area.Width,10000.f);int chars=0,lines=0;while(true){Font mf(family,fs,FontStyleRegular,UnitPixel);gr.MeasureString(content.c_str(),-1,&mf,measureLayout,&fmt,&measured,&chars,&lines);if(v[P_AUTO_FIT]<.5f||measured.Height<=area.Height||fs<=12.f)break;fs-=2.f;}GraphicsPath textPath;textPath.AddString(content.c_str(),-1,family,FontStyleRegular,fs,area,&fmt);
    if(v[P_SHADOW]>.5f){float dx=(v[P_SHADOW_X]-.5f)*80.f,dy=(v[P_SHADOW_Y]-.5f)*80.f,blur=1.f+v[P_SHADOW_BLUR]*25.f;Matrix m;m.Translate(dx,dy);GraphicsPath sh;sh.AddPath(&textPath,FALSE);sh.Transform(&m);Color sc=hsva(v,P_SHADOW_H,P_SHADOW_S,P_SHADOW_B,P_SHADOW_A);for(int i=4;i>=1;i--){Color cc(BYTE(sc.GetA()*v[P_SHADOW_INTENSITY]/i),sc.GetR(),sc.GetG(),sc.GetB());Pen p(cc,blur*i);p.SetLineJoin(LineJoinRound);gr.DrawPath(&p,&sh);}}
    if(v[P_GLOW]>.5f){Color gc=hsva(v,P_GLOW_H,P_GLOW_S,P_GLOW_B,P_GLOW_A);float size=2.f+v[P_GLOW_SIZE]*36.f;for(int i=4;i>=1;i--){Color cc(BYTE(gc.GetA()*v[P_GLOW_INTENSITY]/i),gc.GetR(),gc.GetG(),gc.GetB());Pen p(cc,size*i*.45f);p.SetLineJoin(LineJoinRound);gr.DrawPath(&p,&textPath);}}
    if(v[P_TEXT_STROKE]>.5f){Pen sp(hsva(v,P_STROKE_H,P_STROKE_S,P_STROKE_B,P_STROKE_A),1.f+v[P_TEXT_STROKE_WIDTH]*22.f);sp.SetLineJoin(LineJoinRound);gr.DrawPath(&sp,&textPath);}if(v[P_TEXT_FILL]>.5f){Color ca=hsva(v,P_TEXT_H,P_TEXT_S,P_TEXT_B,P_TEXT_A);if(v[P_GRADIENT]>.5f){Color cb=hsva(v,P_GRADIENT_H,P_GRADIENT_S,P_GRADIENT_B,P_GRADIENT_A);PointF a(area.X,area.Y),b(area.GetRight(),area.Y);int dir=int(v[P_GRADIENT_DIRECTION]+.5f);if(dir==1){b=PointF(area.X,area.GetBottom());}else if(dir==2){b=PointF(area.GetRight(),area.GetBottom());}else if(dir==3){a=PointF(area.X,area.GetBottom());b=PointF(area.GetRight(),area.Y);}LinearGradientBrush gradient(a,b,ca,cb);gr.FillPath(&gradient,&textPath);}else{SolidBrush tb(ca);gr.FillPath(&tb,&textPath);}}
  }
  if(preview&&!text.empty()){std::wstring label=L"NEXT  •  "+std::to_wstring(std::min(current+2,total))+L" / "+std::to_wstring(total);Font lf(L"Arial",18,FontStyleBold,UnitPixel);RectF lr(box.X,std::max(8.f,box.Y-38.f),box.Width,28.f);SolidBrush pill(Color(220,60,190,255));GraphicsPath pp;rounded(pp,RectF(lr.X,lr.Y,170,28),10);gr.FillPath(&pill,&pp);SolidBrush dark(Color(255,10,20,28));gr.DrawString(label.c_str(),-1,&lf,PointF(lr.X+10,lr.Y+3),&dark);}
  Rect lock(0,0,tw,th);BitmapData data{};bmp.LockBits(&lock,ImageLockModeRead,PixelFormat32bppPARGB,&data);GLuint tex=0;GLint old=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old);glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,tw,th,0,GL_BGRA,GL_UNSIGNED_BYTE,data.Scan0);glBindTexture(GL_TEXTURE_2D,GLuint(old));bmp.UnlockBits(&data);return tex;
}

void BRLirik::releaseTextures(){if(currentTexture_)glDeleteTextures(1,&currentTexture_);if(previousTexture_)glDeleteTextures(1,&previousTexture_);if(nextTexture_)glDeleteTextures(1,&nextTexture_);currentTexture_=previousTexture_=nextTexture_=0;}
void BRLirik::rebuildTextures(bool transition){std::string target=cueForRole(false),nextCue;{std::lock_guard<std::mutex>lk(g.mutex);if(role_<.5f&&g.current+1<g.cues.size())nextCue=g.cues[g.current+1];}if(currentTexture_){if(previousTexture_)glDeleteTextures(1,&previousTexture_);previousTexture_=currentTexture_;currentTexture_=0;}else previousCue_=target;if(nextTexture_){glDeleteTextures(1,&nextTexture_);nextTexture_=0;}previousCue_=currentCue_;currentCue_=target;unsigned w=1,h=1;currentTexture_=buildFrameTexture(currentCue_,role_>.5f,w,h);textureWidth_=w;textureHeight_=h;if(!previousTexture_)previousTexture_=buildFrameTexture(previousCue_,role_>.5f,w,h);nextTexture_=buildFrameTexture(nextCue,false,w,h);if(transition){transitionActive_=true;transitionStarted_=std::chrono::steady_clock::now();}else transitionActive_=false;}

FFResult BRLirik::InitGL(const FFGLViewportStruct*v){viewportWidth_=v->width;viewportHeight_=v->height;if(!shader_.Compile(vertexShader,fragmentShader)||!quad_.Initialise())return FF_FAIL;{std::lock_guard<std::mutex>lk(g.mutex);seenRevision_=g.cueRevision;seenStyleRevision_=g.styleRevision;}currentCue_=cueForRole(false);previousCue_=currentCue_;rebuildTextures(false);return CFFGLPlugin::InitGL(v);}
FFResult BRLirik::ProcessOpenGL(ProcessOpenGLStruct*){unsigned long long cr,sr;{std::lock_guard<std::mutex>lk(g.mutex);cr=g.cueRevision;sr=g.styleRevision;}if(cr!=seenRevision_){seenRevision_=cr;seenStyleRevision_=sr;rebuildTextures(true);}else if(sr!=seenStyleRevision_){seenStyleRevision_=sr;releaseTextures();currentCue_=cueForRole(false);previousCue_=currentCue_;rebuildTextures(false);}auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-lastFrame_).count();lastFrame_=now;elapsed_+=std::min(dt,.1);std::vector<float>v;{std::lock_guard<std::mutex>lk(g.mutex);v=g.values;}float duration=.05f+v[P_TRANSITION_DURATION]*2.5f,p=transitionActive_?float(std::chrono::duration<double>(now-transitionStarted_).count()/duration):1.f;if(p>=1){p=1;transitionActive_=false;}ScopedShaderBinding bind(shader_.GetGLID());auto u=[&](const char*n){return shader_.FindUniform(n);};glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,previousTexture_);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,currentTexture_);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,nextTexture_);glUniform1i(u("previousTex"),0);glUniform1i(u("currentTex"),1);glUniform1i(u("nextTex"),2);glUniform1f(u("progress"),p);glUniform1f(u("transitionMode"),v[P_TRANSITION]);glUniform1f(u("smoothness"),v[P_TRANSITION_SMOOTH]);glUniform1f(u("motionMode"),v[P_MOTION]);glUniform1f(u("motionAmount"),v[P_MOTION_AMOUNT]*40.f);glUniform1f(u("motionSpeed"),.25f+v[P_MOTION_SPEED]*5.f);glUniform1f(u("elapsed"),float(elapsed_));glUniform1f(u("stackScale"),v[P_STACK_SCALE]);glUniform1f(u("stackOpacity"),v[P_STACK_OPACITY]);glUniform1f(u("stackGap"),v[P_STACK_GAP]);glUniform1f(u("outputRole"),role_);glUniform2f(u("resolution"),float(viewportWidth_),float(viewportHeight_));glUniform2f(u("anchor"),v[P_POS_X],1.f-v[P_POS_Y]);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);quad_.Draw();glDisable(GL_BLEND);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);return FF_SUCCESS;}
FFResult BRLirik::DeInitGL(){releaseTextures();shader_.FreeGLResources();quad_.Release();return FF_SUCCESS;}

FFResult BRLirik::SetFloatParameter(unsigned i,float x){
  if(i>=P_COUNT)return FF_FAIL;
  bool event=i==P_EDIT||i==P_BROWSE_PNG||i==P_PREVIOUS||i==P_NEXT||i==P_GOTO||i==P_RESET||i==P_REFRESH_FONTS;
  if(event&&x==0)return FF_SUCCESS;
  if(i==P_ROLE){role_=x;seenRevision_=0;return FF_SUCCESS;}
  if(i==P_EDIT){{std::lock_guard<std::mutex>lk(g.mutex);g.values[P_INPUT_MODE]=0;rebuildActiveCuesLocked();}openEditor();return FF_SUCCESS;}
  if(i==P_BROWSE_PNG){
    auto files=choosePngFiles();if(files.empty())return FF_SUCCESS;
    std::string saved;std::lock_guard<std::mutex>lk(g.mutex);g.pngFiles.clear();for(const auto&p:files){g.pngFiles.push_back(w8(p));if(!saved.empty())saved+='\n';saved+=g.pngFiles.back();}writeUtf8(pngListPath(),saved);g.values[P_INPUT_MODE]=1;rebuildActiveCuesLocked();return FF_SUCCESS;
  }
  if(i==P_PREVIOUS){
    bool locked=false;{std::lock_guard<std::mutex>lk(g.mutex);locked=g.values[P_TRANSITION_LOCK]>.5f;}
    if(locked&&transitionActive_)return FF_SUCCESS;
    std::lock_guard<std::mutex>lk(g.mutex);if(g.current>0)g.current--;g.cueRevision++;return FF_SUCCESS;
  }
  if(i==P_NEXT){
    bool locked=false;{std::lock_guard<std::mutex>lk(g.mutex);locked=g.values[P_TRANSITION_LOCK]>.5f;}
    if(locked&&transitionActive_)return FF_SUCCESS;
    std::lock_guard<std::mutex>lk(g.mutex);if(g.current+1<g.cues.size())g.current++;g.cueRevision++;return FF_SUCCESS;
  }
  if(i==P_RESET){std::lock_guard<std::mutex>lk(g.mutex);g.current=0;g.cueRevision++;return FF_SUCCESS;}
  if(i==P_GOTO){
    try{size_t n=std::max(1,std::stoi(gotoText_));std::lock_guard<std::mutex>lk(g.mutex);g.current=std::min(n-1,g.cues.size()-1);g.cueRevision++;}catch(...){ }
    return FF_SUCCESS;
  }
  if(i==P_REFRESH_FONTS){enumerateFonts();return FF_SUCCESS;}
  if(i==P_FONT_LIST&&!fonts_.empty()){
    unsigned n=std::min<unsigned>(unsigned(x+.5f),unsigned(fonts_.size()-1));
    std::lock_guard<std::mutex>lk(g.mutex);g.font=fonts_[n];fontName_=g.font;g.styleRevision++;return FF_SUCCESS;
  }
  std::lock_guard<std::mutex>lk(g.mutex);
  g.values[i]=x;
  if(i==P_INPUT_MODE){rebuildActiveCuesLocked();}
  else if(i==P_SHOW_PREVIEW){g.preview=x>.5f;g.cueRevision++;}
  else if(i==P_BLANK_EDGES){rebuildActiveCuesLocked();}
  else if(i==P_POSITION_PRESET){
    int preset=int(x+.5f);
    if(preset==1){g.values[P_POS_X]=.5f;g.values[P_POS_Y]=.18f;}
    else if(preset==2){g.values[P_POS_X]=.5f;g.values[P_POS_Y]=.5f;}
    else if(preset==3){g.values[P_POS_X]=.5f;g.values[P_POS_Y]=.82f;}
    g.styleRevision++;
  }
  else g.styleRevision++;
  return FF_SUCCESS;
}
float BRLirik::GetFloatParameter(unsigned i){if(i==P_ROLE)return role_;std::lock_guard<std::mutex>lk(g.mutex);return i<P_COUNT?g.values[i]:0;}
FFResult BRLirik::SetTextParameter(unsigned i,const char*x){std::string s=x?x:"";if(i==P_GOTO_NUMBER){gotoText_=s;return FF_SUCCESS;}if(i==P_FONT_NAME){fontName_=s.empty()?"Arial":s;std::lock_guard<std::mutex>lk(g.mutex);g.font=fontName_;g.styleRevision++;return FF_SUCCESS;}if(i==P_CREDIT||i==P_STATUS)return FF_SUCCESS;return FF_FAIL;}
char* BRLirik::GetTextParameter(unsigned i){static thread_local char b[8192]{};std::string s;if(i==P_CREDIT)s=credit_;else if(i==P_GOTO_NUMBER)s=gotoText_;else if(i==P_FONT_NAME){std::lock_guard<std::mutex>lk(g.mutex);s=g.font;}else if(i==P_STATUS){std::lock_guard<std::mutex>lk(g.mutex);size_t program=g.cues.empty()?0:g.current+1,next=g.cues.empty()?0:std::min(g.current+2,g.cues.size());s=(role_>.5f?"NEXT ":"PROGRAM ")+std::to_string(role_>.5f?next:program)+" / "+std::to_string(g.cues.size());}size_t n=std::min(s.size(),sizeof(b)-1);memcpy(b,s.data(),n);b[n]=0;return b;}
