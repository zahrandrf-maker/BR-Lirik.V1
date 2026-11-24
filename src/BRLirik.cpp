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
  P_ROLE,P_EDIT,P_PREVIOUS,P_NEXT,P_GOTO,P_GOTO_NUMBER,P_RESET,P_SHOW_PREVIEW,P_STATUS,
  P_TRANSITION,P_TRANSITION_DURATION,P_TRANSITION_SMOOTH,
  P_MOTION,P_MOTION_AMOUNT,P_MOTION_SPEED,
  P_FONT_NAME,P_FONT_LIST,P_REFRESH_FONTS,P_FONT_SIZE,P_AUTO_FIT,P_ALIGN,P_VALIGN,P_LINE_SPACING,
  P_TEXT_H,P_TEXT_S,P_TEXT_B,P_TEXT_A,P_TEXT_FILL,
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
bool readUtf8(const std::wstring&,std::string&);

static CFFGLPluginInfo PluginInfo(PluginFactory<BRLirik>,"BRLK","BR Lirik",2,1,0,101,FF_SOURCE,"Live lyrics source by BELAJAR RESOLUME","Effect by Belajar Resolume");

float c01(float x){return std::max(0.f,std::min(1.f,x));}
std::wstring u8w(const std::string& s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);if(n<=0)return std::wstring(s.begin(),s.end());std::wstring r(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,r.data(),n);r.pop_back();return r;}
std::string w8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,nullptr,0,nullptr,nullptr);std::string r(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,r.data(),n,nullptr,nullptr);r.pop_back();return r;}
std::vector<std::string> splitCues(std::string text){std::vector<std::string> out;std::string line;std::stringstream ss(text);while(std::getline(ss,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();out.push_back(line);}if(out.empty())out.push_back("");return out;}
void initShared(){
  std::string saved;if(readUtf8(autoSavePath(),saved)&&!saved.empty())g.raw=saved;
  g.cues=splitCues(g.raw);g.values.assign(P_COUNT,0.f);
  auto&v=g.values;v[P_SHOW_PREVIEW]=1;v[P_TRANSITION]=1;v[P_TRANSITION_DURATION]=.18f;v[P_TRANSITION_SMOOTH]=.8f;
  v[P_MOTION]=0;v[P_MOTION_AMOUNT]=.18f;v[P_MOTION_SPEED]=.18f;v[P_FONT_SIZE]=.24f;v[P_AUTO_FIT]=1;v[P_ALIGN]=1;v[P_VALIGN]=1;v[P_LINE_SPACING]=.5f;
  v[P_TEXT_B]=1;v[P_TEXT_A]=1;v[P_TEXT_FILL]=1;v[P_TEXT_STROKE]=1;v[P_TEXT_STROKE_WIDTH]=.10f;v[P_STROKE_B]=0;v[P_STROKE_A]=1;
  v[P_SHADOW]=1;v[P_SHADOW_INTENSITY]=.45f;v[P_SHADOW_BLUR]=.18f;v[P_SHADOW_X]=.53f;v[P_SHADOW_Y]=.55f;v[P_SHADOW_B]=0;v[P_SHADOW_A]=.8f;
  v[P_GLOW_INTENSITY]=.3f;v[P_GLOW_SIZE]=.18f;v[P_GLOW_B]=1;v[P_GLOW_A]=.75f;
  v[P_BOX_MODE]=0;v[P_BOX_WIDTH]=.72f;v[P_BOX_HEIGHT]=.24f;v[P_PAD_X]=.08f;v[P_PAD_Y]=.12f;v[P_POS_X]=.5f;v[P_POS_Y]=.78f;v[P_RADIUS]=.18f;
  v[P_BOX_B]=.05f;v[P_BOX_A]=.72f;v[P_BOX_STROKE_WIDTH]=.08f;v[P_BOX_STROKE_B]=1;v[P_BOX_STROKE_A]=1;
}

int CALLBACK enumFont(const LOGFONTW*lf,const TEXTMETRICW*,DWORD,LPARAM p){auto*s=reinterpret_cast<std::set<std::wstring>*>(p);if(lf->lfFaceName[0]!=L'@')s->insert(lf->lfFaceName);return 1;}
Color hsva(const std::vector<float>&v,int h,int s,int b,int a){float rgb[3];HSVtoRGB(v[h],v[s],v[b],rgb[0],rgb[1],rgb[2]);return Color(BYTE(c01(v[a])*255),BYTE(c01(rgb[0])*255),BYTE(c01(rgb[1])*255),BYTE(c01(rgb[2])*255));}
void rounded(GraphicsPath&path,const RectF&r,float rad){float d=std::min(rad*2.f,std::min(r.Width,r.Height));if(d<=1){path.AddRectangle(r);return;}path.AddArc(r.X,r.Y,d,d,180,90);path.AddArc(r.GetRight()-d,r.Y,d,d,270,90);path.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);path.AddArc(r.X,r.GetBottom()-d,d,d,90,90);path.CloseFigure();}
std::wstring autoSavePath(){wchar_t base[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);std::wstring root=n?base:L".";root+=L"\\Belajar Resolume";CreateDirectoryW(root.c_str(),nullptr);root+=L"\\BR Lirik";CreateDirectoryW(root.c_str(),nullptr);return root+L"\\autosave.txt";}
bool writeUtf8(const std::wstring&path,const std::string&data){std::ofstream f(std::filesystem::path(path),std::ios::binary);if(!f)return false;f.write(data.data(),std::streamsize(data.size()));return bool(f);}
bool readUtf8(const std::wstring&path,std::string&data){std::ifstream f(std::filesystem::path(path),std::ios::binary);if(!f)return false;data.assign(std::istreambuf_iterator<char>(f),{});if(data.size()>=3&&(unsigned char)data[0]==0xEF&&(unsigned char)data[1]==0xBB&&(unsigned char)data[2]==0xBF)data.erase(0,3);return true;}
std::wstring chooseFile(HWND owner,bool save){wchar_t file[MAX_PATH]=L"BR-Lirik.txt";OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=owner;o.lpstrFilter=L"Text File (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";o.lpstrFile=file;o.nMaxFile=MAX_PATH;o.lpstrDefExt=L"txt";o.Flags=OFN_EXPLORER|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);BOOL ok=save?GetSaveFileNameW(&o):GetOpenFileNameW(&o);return ok?std::wstring(file):L"";}

const char* vertexShader=R"GLSL(#version 410 core
layout(location=0)in vec4 vPosition;layout(location=1)in vec2 vUV;out vec2 uv;void main(){gl_Position=vPosition;uv=vUV;})GLSL";
const char* fragmentShader=R"GLSL(#version 410 core
uniform sampler2D currentTex,previousTex;uniform float progress,transitionMode,smoothness,motionMode,motionAmount,motionSpeed,elapsed;uniform vec2 resolution;in vec2 uv;out vec4 fragColor;
vec4 sampleTex(sampler2D t,vec2 p){if(any(lessThan(p,vec2(0)))||any(greaterThan(p,vec2(1))))return vec4(0);return texture(t,vec2(p.x,1.-p.y));}
void main(){float p=clamp(progress,0.,1.);p=mix(p,p*p*(3.-2.*p),smoothness);vec2 drift=vec2(0);if(motionMode>0.5&&motionMode<1.5)drift=vec2(sin(elapsed*motionSpeed),cos(elapsed*motionSpeed*.73))*motionAmount/resolution;if(motionMode>1.5)drift=vec2(sin(elapsed*motionSpeed*17.),sin(elapsed*motionSpeed*23.7))*motionAmount/resolution;vec2 op=drift,np=drift;float oa=1.,na=1.;if(transitionMode<.5){oa=0.;na=1.;}else if(transitionMode<1.5){oa=1.-p;na=p;}else if(transitionMode<2.5){op+=vec2(0,-p);np+=vec2(0,1.-p);oa=1.-p;na=p;}else{op+=vec2(-p,0);np+=vec2(1.-p,0);oa=1.-p;na=p;}vec4 oldC=sampleTex(previousTex,uv-op)*oa,newC=sampleTex(currentTex,uv-np)*na;fragColor=newC+oldC*(1.-newC.a);})GLSL";
}

BRLirik::BRLirik(){
  std::call_once(initFlag,initShared);SetMinInputs(0);SetMaxInputs(0);GdiplusStartupInput si;GdiplusStartup(&gdiplusToken_,&si,nullptr);enumerateFonts();
  SetOptionParamInfo(P_ROLE,"Output Role",2,0);addOption(P_ROLE,0,"PROGRAM",0);addOption(P_ROLE,1,"PREVIEW - NEXT",1);
  SetParamInfof(P_EDIT,"Add / Edit Lyrics",FF_TYPE_EVENT);SetParamInfof(P_PREVIOUS,"Previous",FF_TYPE_EVENT);SetParamInfof(P_NEXT,"Next",FF_TYPE_EVENT);SetParamInfof(P_GOTO,"Go To",FF_TYPE_EVENT);SetParamInfo(P_GOTO_NUMBER,"Go To Line Number",FF_TYPE_TEXT,gotoText_.c_str());SetParamInfof(P_RESET,"Reset to Line 1",FF_TYPE_EVENT);SetParamInfof(P_SHOW_PREVIEW,"Show Preview Output",FF_TYPE_BOOLEAN);SetParamInfo(P_STATUS,"Lyric Status",FF_TYPE_TEXT,"Line 1 / 1");
  SetOptionParamInfo(P_TRANSITION,"Transition",4,1);const char*t[]={"Cut To","Fade In / Out","Roll Up + Fade","Slide Left + Fade"};for(unsigned i=0;i<4;i++)addOption(P_TRANSITION,i,t[i],float(i));SetParamInfof(P_TRANSITION_DURATION,"Transition Duration",FF_TYPE_STANDARD);SetParamInfof(P_TRANSITION_SMOOTH,"Transition Smoothness",FF_TYPE_STANDARD);
  SetOptionParamInfo(P_MOTION,"Live Motion",3,0);addOption(P_MOTION,0,"None",0);addOption(P_MOTION,1,"Slow Floating",1);addOption(P_MOTION,2,"Rock Shake",2);SetParamInfof(P_MOTION_AMOUNT,"Motion Amount",FF_TYPE_STANDARD);SetParamInfof(P_MOTION_SPEED,"Motion Speed",FF_TYPE_STANDARD);
  SetParamInfo(P_FONT_NAME,"Font Search / Name",FF_TYPE_TEXT,fontName_.c_str());SetOptionParamInfo(P_FONT_LIST,"Installed Font",unsigned(std::max<size_t>(fonts_.size(),1)),0);if(fonts_.empty())addOption(P_FONT_LIST,0,"Arial",0);else for(unsigned i=0;i<fonts_.size();i++)addOption(P_FONT_LIST,i,fonts_[i].c_str(),float(i));SetParamInfof(P_REFRESH_FONTS,"Refresh Fonts",FF_TYPE_EVENT);SetParamInfof(P_FONT_SIZE,"Font Size",FF_TYPE_STANDARD);SetParamInfof(P_AUTO_FIT,"Auto Fit Height",FF_TYPE_BOOLEAN);SetOptionParamInfo(P_ALIGN,"Horizontal Align",3,1);addOption(P_ALIGN,0,"Left",0);addOption(P_ALIGN,1,"Center",1);addOption(P_ALIGN,2,"Right",2);SetOptionParamInfo(P_VALIGN,"Vertical Align",3,1);addOption(P_VALIGN,0,"Top",0);addOption(P_VALIGN,1,"Middle",1);addOption(P_VALIGN,2,"Bottom",2);SetParamInfof(P_LINE_SPACING,"Line Spacing",FF_TYPE_STANDARD);
  SetParamInfof(P_TEXT_H,"Text Color",FF_TYPE_HUE);SetParamInfof(P_TEXT_S,"Text Saturation",FF_TYPE_SATURATION);SetParamInfof(P_TEXT_B,"Text Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_TEXT_A,"Text Opacity",FF_TYPE_ALPHA);SetParamInfof(P_TEXT_FILL,"Text Fill",FF_TYPE_BOOLEAN);SetParamInfof(P_TEXT_STROKE,"Text Stroke",FF_TYPE_BOOLEAN);SetParamInfof(P_TEXT_STROKE_WIDTH,"Stroke Width",FF_TYPE_STANDARD);SetParamInfof(P_STROKE_H,"Stroke Color",FF_TYPE_HUE);SetParamInfof(P_STROKE_S,"Stroke Saturation",FF_TYPE_SATURATION);SetParamInfof(P_STROKE_B,"Stroke Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_STROKE_A,"Stroke Opacity",FF_TYPE_ALPHA);
  SetParamInfof(P_SHADOW,"Drop Shadow",FF_TYPE_BOOLEAN);SetParamInfof(P_SHADOW_INTENSITY,"Shadow Intensity",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_BLUR,"Shadow Softness",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_X,"Shadow Offset X",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_Y,"Shadow Offset Y",FF_TYPE_STANDARD);SetParamInfof(P_SHADOW_H,"Shadow Color",FF_TYPE_HUE);SetParamInfof(P_SHADOW_S,"Shadow Saturation",FF_TYPE_SATURATION);SetParamInfof(P_SHADOW_B,"Shadow Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_SHADOW_A,"Shadow Opacity",FF_TYPE_ALPHA);
  SetParamInfof(P_GLOW,"Glow",FF_TYPE_BOOLEAN);SetParamInfof(P_GLOW_INTENSITY,"Glow Intensity",FF_TYPE_STANDARD);SetParamInfof(P_GLOW_SIZE,"Glow Size",FF_TYPE_STANDARD);SetParamInfof(P_GLOW_H,"Glow Color",FF_TYPE_HUE);SetParamInfof(P_GLOW_S,"Glow Saturation",FF_TYPE_SATURATION);SetParamInfof(P_GLOW_B,"Glow Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_GLOW_A,"Glow Opacity",FF_TYPE_ALPHA);
  SetOptionParamInfo(P_BOX_MODE,"Text Box",4,0);addOption(P_BOX_MODE,0,"Off",0);addOption(P_BOX_MODE,1,"Fill",1);addOption(P_BOX_MODE,2,"Fill + Stroke",2);addOption(P_BOX_MODE,3,"Stroke Only",3);SetParamInfof(P_BOX_WIDTH,"Box Width",FF_TYPE_STANDARD);SetParamInfof(P_BOX_HEIGHT,"Box Height",FF_TYPE_STANDARD);SetParamInfof(P_PAD_X,"Horizontal Padding",FF_TYPE_STANDARD);SetParamInfof(P_PAD_Y,"Vertical Padding",FF_TYPE_STANDARD);SetParamInfof(P_POS_X,"Position X",FF_TYPE_XPOS);SetParamInfof(P_POS_Y,"Position Y",FF_TYPE_YPOS);SetParamInfof(P_RADIUS,"Round Box",FF_TYPE_STANDARD);SetParamInfof(P_BOX_H,"Box Color",FF_TYPE_HUE);SetParamInfof(P_BOX_S,"Box Saturation",FF_TYPE_SATURATION);SetParamInfof(P_BOX_B,"Box Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_BOX_A,"Box Opacity",FF_TYPE_ALPHA);SetParamInfof(P_BOX_STROKE_WIDTH,"Box Stroke Width",FF_TYPE_STANDARD);SetParamInfof(P_BOX_STROKE_H,"Box Stroke Color",FF_TYPE_HUE);SetParamInfof(P_BOX_STROKE_S,"Box Stroke Saturation",FF_TYPE_SATURATION);SetParamInfof(P_BOX_STROKE_B,"Box Stroke Brightness",FF_TYPE_BRIGHTNESS);SetParamInfof(P_BOX_STROKE_A,"Box Stroke Opacity",FF_TYPE_ALPHA);SetParamInfo(P_CREDIT,"Effect by Belajar Resolume",FF_TYPE_TEXT,credit_.c_str());
  for(unsigned i=P_ROLE;i<=P_STATUS;i++)SetParamGroup(i,"Live Control");for(unsigned i=P_TRANSITION;i<=P_TRANSITION_SMOOTH;i++)SetParamGroup(i,"Transition");for(unsigned i=P_MOTION;i<=P_MOTION_SPEED;i++)SetParamGroup(i,"Live Motion");for(unsigned i=P_FONT_NAME;i<=P_LINE_SPACING;i++)SetParamGroup(i,"Typography");for(unsigned i=P_TEXT_H;i<=P_STROKE_A;i++)SetParamGroup(i,"Text Style");for(unsigned i=P_SHADOW;i<=P_SHADOW_A;i++)SetParamGroup(i,"Drop Shadow");for(unsigned i=P_GLOW;i<=P_GLOW_A;i++)SetParamGroup(i,"Glow");for(unsigned i=P_BOX_MODE;i<=P_BOX_STROKE_A;i++)SetParamGroup(i,"Text Box");SetParamGroup(P_CREDIT,"Credit");lastFrame_=std::chrono::steady_clock::now();
}

BRLirik::~BRLirik(){editorStopRequested_=true;if(HWND w=editorWindow_.load())PostMessageW(w,WM_CLOSE,0,0);if(editorThread_.joinable())editorThread_.join();releaseTextures();if(gdiplusToken_)GdiplusShutdown(gdiplusToken_);}
void BRLirik::addOption(unsigned p,unsigned e,const char*n,float v){SetParamElementInfo(p,e,n,v);}
void BRLirik::enumerateFonts(){std::set<std::wstring>names;HDC dc=GetDC(nullptr);LOGFONTW lf{};lf.lfCharSet=DEFAULT_CHARSET;EnumFontFamiliesExW(dc,&lf,reinterpret_cast<FONTENUMPROCW>(enumFont),reinterpret_cast<LPARAM>(&names),0);ReleaseDC(nullptr,dc);fonts_.clear();for(auto&x:names)fonts_.push_back(w8(x));}

void BRLirik::openEditor(){if(editorRunning_){if(HWND w=editorWindow_.load())PostMessageW(w,WM_APP+1,0,0);return;}if(editorThread_.joinable())editorThread_.join();editorStopRequested_=false;editorRunning_=true;editorThread_=std::thread(&BRLirik::runEditor,this);}
void BRLirik::runEditor(){static const wchar_t*cn=L"BRLirikEditorWindow";HINSTANCE hi=GetModuleHandleW(nullptr);WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=EditorWndProc;wc.hInstance=hi;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=cn;if(RegisterClassExW(&wc)==0&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){editorRunning_=false;return;}HWND w=CreateWindowExW(WS_EX_TOOLWINDOW,cn,L"BR Lirik — Lyrics Editor",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,860,620,nullptr,nullptr,hi,this);if(!w){editorRunning_=false;return;}editorWindow_=w;MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}editorWindow_=nullptr;editorRunning_=false;}

LRESULT CALLBACK BRLirik::EditorWndProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){auto*self=reinterpret_cast<BRLirik*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(msg==WM_NCCREATE){auto*c=reinterpret_cast<CREATESTRUCTW*>(lp);self=reinterpret_cast<BRLirik*>(c->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}if(!self)return DefWindowProcW(w,msg,wp,lp);constexpr int TXT=1001,UPDATE=1002,LOAD=1003,SAVE=1004,CLEAR=1005,CANCEL=1006;switch(msg){case WM_CREATE:{CreateWindowExW(0,L"STATIC",L"Satu baris input = satu cue lirik. Enter membuat cue berikutnya. Baris kosong = layar lirik kosong.",WS_CHILD|WS_VISIBLE,16,14,800,22,w,nullptr,nullptr,nullptr);self->editorTextBox_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_LEFT|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,16,42,810,470,w,reinterpret_cast<HMENU>(TXT),nullptr,nullptr);const wchar_t*labels[]={L"Load TXT",L"Save TXT",L"Clear",L"Cancel",L"Update Lyrics"};int ids[]={LOAD,SAVE,CLEAR,CANCEL,UPDATE};for(int i=0;i<5;i++){HWND b=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|(i==4?BS_DEFPUSHBUTTON:BS_PUSHBUTTON),16+i*130,526,i==4?142:112,34,w,reinterpret_cast<HMENU>(ids[i]),nullptr,nullptr);SendMessageW(b,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);}std::string raw;{std::lock_guard<std::mutex>lk(g.mutex);raw=g.raw;}auto wide=u8w(raw);SetWindowTextW(self->editorTextBox_,wide.c_str());SendMessageW(self->editorTextBox_,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);SetFocus(self->editorTextBox_);return 0;}case WM_SIZE:{int W=LOWORD(lp),H=HIWORD(lp);if(self->editorTextBox_)MoveWindow(self->editorTextBox_,16,42,std::max(240,W-32),std::max(80,H-108),TRUE);int y=std::max(90,H-50);int x=16;for(int id:{LOAD,SAVE,CLEAR,CANCEL,UPDATE}){HWND b=GetDlgItem(w,id);int bw=id==UPDATE?142:112;if(b)MoveWindow(b,x,y,bw,34,TRUE);x+=bw+12;}return 0;}case WM_COMMAND:{int id=LOWORD(wp);auto textFromBox=[&](){int n=GetWindowTextLengthW(self->editorTextBox_);std::wstring s(size_t(n)+1,L'\0');GetWindowTextW(self->editorTextBox_,s.data(),n+1);s.resize(size_t(n));return w8(s);};if(id==LOAD){auto p=chooseFile(w,false);std::string data;if(!p.empty()&&readUtf8(p,data)){auto wide=u8w(data);SetWindowTextW(self->editorTextBox_,wide.c_str());}return 0;}if(id==SAVE){auto p=chooseFile(w,true);if(!p.empty())writeUtf8(p,textFromBox());return 0;}if(id==CLEAR){SetWindowTextW(self->editorTextBox_,L"");return 0;}if(id==UPDATE){std::string raw=textFromBox();{std::lock_guard<std::mutex>lk(g.mutex);g.raw=raw;g.cues=splitCues(raw);g.current=std::min(g.current,g.cues.size()-1);g.cueRevision++;}writeUtf8(autoSavePath(),raw);DestroyWindow(w);return 0;}if(id==CANCEL){DestroyWindow(w);return 0;}break;}case WM_APP+1:ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);SetFocus(self->editorTextBox_);return 0;case WM_CLOSE:DestroyWindow(w);return 0;case WM_NCDESTROY:self->editorTextBox_=nullptr;self->editorWindow_=nullptr;SetWindowLongPtrW(w,GWLP_USERDATA,0);PostQuitMessage(0);break;}return DefWindowProcW(w,msg,wp,lp);}

std::string BRLirik::cueForRole(bool previous)const{std::lock_guard<std::mutex>lk(g.mutex);if(g.cues.empty())return "";size_t index=g.current;if(role_>.5f){if(!g.preview||g.current+1>=g.cues.size())return "";index=g.current+1;}if(previous&&index>0)index--;return g.cues[index];}

unsigned int BRLirik::buildFrameTexture(const std::string&text,bool preview,unsigned int&tw,unsigned int&th){
  std::vector<float>v;std::string font;size_t current=0,total=1;{std::lock_guard<std::mutex>lk(g.mutex);v=g.values;font=g.font;current=g.current;total=g.cues.size();}
  tw=std::max(2u,viewportWidth_);th=std::max(2u,viewportHeight_);Bitmap bmp(tw,th,PixelFormat32bppPARGB);Graphics gr(&bmp);gr.SetSmoothingMode(SmoothingModeAntiAlias);gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);gr.Clear(Color(0,0,0,0));
  float bw=std::max(120.f,v[P_BOX_WIDTH]*tw),bh=std::max(60.f,v[P_BOX_HEIGHT]*th),cx=v[P_POS_X]*tw,cy=v[P_POS_Y]*th;RectF box(cx-bw*.5f,cy-bh*.5f,bw,bh);float radius=v[P_RADIUS]*std::min(bw,bh)*.5f;GraphicsPath boxPath;rounded(boxPath,box,radius);int bm=int(v[P_BOX_MODE]+.5f);if(bm==1||bm==2){SolidBrush bb(hsva(v,P_BOX_H,P_BOX_S,P_BOX_B,P_BOX_A));gr.FillPath(&bb,&boxPath);}if(bm==2||bm==3){Pen bp(hsva(v,P_BOX_STROKE_H,P_BOX_STROKE_S,P_BOX_STROKE_B,P_BOX_STROKE_A),1.f+v[P_BOX_STROKE_WIDTH]*24.f);bp.SetLineJoin(LineJoinRound);gr.DrawPath(&bp,&boxPath);}
  float px=8.f+v[P_PAD_X]*bw*.35f,py=5.f+v[P_PAD_Y]*bh*.35f;RectF area(box.X+px,box.Y+py,std::max(20.f,box.Width-2*px),std::max(20.f,box.Height-2*py));std::wstring content=u8w(text.empty()?" ":text),familyW=u8w(font);FontFamily selected(familyW.c_str()),fallback(L"Arial");FontFamily*family=selected.IsAvailable()?&selected:&fallback;StringFormat fmt(StringFormat::GenericTypographic());fmt.SetTrimming(StringTrimmingWord);fmt.SetFormatFlags(0);if(v[P_ALIGN]<.5f)fmt.SetAlignment(StringAlignmentNear);else if(v[P_ALIGN]<1.5f)fmt.SetAlignment(StringAlignmentCenter);else fmt.SetAlignment(StringAlignmentFar);if(v[P_VALIGN]<.5f)fmt.SetLineAlignment(StringAlignmentNear);else if(v[P_VALIGN]<1.5f)fmt.SetLineAlignment(StringAlignmentCenter);else fmt.SetLineAlignment(StringAlignmentFar);
  float fs=18.f+v[P_FONT_SIZE]*190.f;RectF measured,measureLayout(0.f,0.f,area.Width,10000.f);int chars=0,lines=0;while(true){Font mf(family,fs,FontStyleRegular,UnitPixel);gr.MeasureString(content.c_str(),-1,&mf,measureLayout,&fmt,&measured,&chars,&lines);if(v[P_AUTO_FIT]<.5f||measured.Height<=area.Height||fs<=12.f)break;fs-=2.f;}GraphicsPath textPath;textPath.AddString(content.c_str(),-1,family,FontStyleRegular,fs,area,&fmt);
  if(v[P_SHADOW]>.5f){float dx=(v[P_SHADOW_X]-.5f)*80.f,dy=(v[P_SHADOW_Y]-.5f)*80.f,blur=1.f+v[P_SHADOW_BLUR]*25.f;Matrix m;m.Translate(dx,dy);GraphicsPath sh;sh.AddPath(&textPath,FALSE);sh.Transform(&m);Color sc=hsva(v,P_SHADOW_H,P_SHADOW_S,P_SHADOW_B,P_SHADOW_A);for(int i=4;i>=1;i--){Color cc(BYTE(sc.GetA()*v[P_SHADOW_INTENSITY]/i),sc.GetR(),sc.GetG(),sc.GetB());Pen p(cc,blur*i);p.SetLineJoin(LineJoinRound);gr.DrawPath(&p,&sh);}}
  if(v[P_GLOW]>.5f){Color gc=hsva(v,P_GLOW_H,P_GLOW_S,P_GLOW_B,P_GLOW_A);float size=2.f+v[P_GLOW_SIZE]*36.f;for(int i=4;i>=1;i--){Color cc(BYTE(gc.GetA()*v[P_GLOW_INTENSITY]/i),gc.GetR(),gc.GetG(),gc.GetB());Pen p(cc,size*i*.45f);p.SetLineJoin(LineJoinRound);gr.DrawPath(&p,&textPath);}}
  if(v[P_TEXT_STROKE]>.5f){Pen sp(hsva(v,P_STROKE_H,P_STROKE_S,P_STROKE_B,P_STROKE_A),1.f+v[P_TEXT_STROKE_WIDTH]*22.f);sp.SetLineJoin(LineJoinRound);gr.DrawPath(&sp,&textPath);}if(v[P_TEXT_FILL]>.5f){SolidBrush tb(hsva(v,P_TEXT_H,P_TEXT_S,P_TEXT_B,P_TEXT_A));gr.FillPath(&tb,&textPath);}
  if(preview&&!text.empty()){std::wstring label=L"NEXT  •  "+std::to_wstring(std::min(current+2,total))+L" / "+std::to_wstring(total);Font lf(L"Arial",18,FontStyleBold,UnitPixel);RectF lr(box.X,std::max(8.f,box.Y-38.f),box.Width,28.f);SolidBrush pill(Color(220,60,190,255));GraphicsPath pp;rounded(pp,RectF(lr.X,lr.Y,170,28),10);gr.FillPath(&pill,&pp);SolidBrush dark(Color(255,10,20,28));gr.DrawString(label.c_str(),-1,&lf,PointF(lr.X+10,lr.Y+3),&dark);}
  Rect lock(0,0,tw,th);BitmapData data{};bmp.LockBits(&lock,ImageLockModeRead,PixelFormat32bppPARGB,&data);GLuint tex=0;GLint old=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old);glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,tw,th,0,GL_BGRA,GL_UNSIGNED_BYTE,data.Scan0);glBindTexture(GL_TEXTURE_2D,GLuint(old));bmp.UnlockBits(&data);return tex;
}

void BRLirik::releaseTextures(){if(currentTexture_)glDeleteTextures(1,&currentTexture_);if(previousTexture_)glDeleteTextures(1,&previousTexture_);currentTexture_=previousTexture_=0;}
void BRLirik::rebuildTextures(bool transition){std::string target=cueForRole(false);if(currentTexture_){if(previousTexture_)glDeleteTextures(1,&previousTexture_);previousTexture_=currentTexture_;currentTexture_=0;}else previousCue_=target;previousCue_=currentCue_;currentCue_=target;unsigned w=1,h=1;currentTexture_=buildFrameTexture(currentCue_,role_>.5f,w,h);textureWidth_=w;textureHeight_=h;if(!previousTexture_)previousTexture_=buildFrameTexture(previousCue_,role_>.5f,w,h);if(transition){transitionActive_=true;transitionStarted_=std::chrono::steady_clock::now();}else transitionActive_=false;}

FFResult BRLirik::InitGL(const FFGLViewportStruct*v){viewportWidth_=v->width;viewportHeight_=v->height;if(!shader_.Compile(vertexShader,fragmentShader)||!quad_.Initialise())return FF_FAIL;{std::lock_guard<std::mutex>lk(g.mutex);seenRevision_=g.cueRevision;seenStyleRevision_=g.styleRevision;}currentCue_=cueForRole(false);previousCue_=currentCue_;rebuildTextures(false);return CFFGLPlugin::InitGL(v);}
FFResult BRLirik::ProcessOpenGL(ProcessOpenGLStruct*){unsigned long long cr,sr;{std::lock_guard<std::mutex>lk(g.mutex);cr=g.cueRevision;sr=g.styleRevision;}if(cr!=seenRevision_){seenRevision_=cr;seenStyleRevision_=sr;rebuildTextures(true);}else if(sr!=seenStyleRevision_){seenStyleRevision_=sr;releaseTextures();currentCue_=cueForRole(false);previousCue_=currentCue_;rebuildTextures(false);}auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-lastFrame_).count();lastFrame_=now;elapsed_+=std::min(dt,.1);std::vector<float>v;{std::lock_guard<std::mutex>lk(g.mutex);v=g.values;}float duration=.05f+v[P_TRANSITION_DURATION]*2.5f,p=transitionActive_?float(std::chrono::duration<double>(now-transitionStarted_).count()/duration):1.f;if(p>=1){p=1;transitionActive_=false;}ScopedShaderBinding bind(shader_.GetGLID());auto u=[&](const char*n){return shader_.FindUniform(n);};glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,previousTexture_);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,currentTexture_);glUniform1i(u("previousTex"),0);glUniform1i(u("currentTex"),1);glUniform1f(u("progress"),p);glUniform1f(u("transitionMode"),v[P_TRANSITION]);glUniform1f(u("smoothness"),v[P_TRANSITION_SMOOTH]);glUniform1f(u("motionMode"),v[P_MOTION]);glUniform1f(u("motionAmount"),v[P_MOTION_AMOUNT]*40.f);glUniform1f(u("motionSpeed"),.25f+v[P_MOTION_SPEED]*5.f);glUniform1f(u("elapsed"),float(elapsed_));glUniform2f(u("resolution"),float(viewportWidth_),float(viewportHeight_));glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);quad_.Draw();glDisable(GL_BLEND);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);return FF_SUCCESS;}
FFResult BRLirik::DeInitGL(){releaseTextures();shader_.FreeGLResources();quad_.Release();return FF_SUCCESS;}

FFResult BRLirik::SetFloatParameter(unsigned i,float x){if(i>=P_COUNT)return FF_FAIL;if(i==P_ROLE){role_=x;seenRevision_=0;return FF_SUCCESS;}if(i==P_EDIT&&x){openEditor();return FF_SUCCESS;}if(i==P_PREVIOUS&&x){std::lock_guard<std::mutex>lk(g.mutex);if(g.current>0)g.current--;g.cueRevision++;return FF_SUCCESS;}if(i==P_NEXT&&x){std::lock_guard<std::mutex>lk(g.mutex);if(g.current+1<g.cues.size())g.current++;g.cueRevision++;return FF_SUCCESS;}if(i==P_RESET&&x){std::lock_guard<std::mutex>lk(g.mutex);g.current=0;g.cueRevision++;return FF_SUCCESS;}if(i==P_GOTO&&x){try{size_t n=std::max(1,std::stoi(gotoText_));std::lock_guard<std::mutex>lk(g.mutex);g.current=std::min(n-1,g.cues.size()-1);g.cueRevision++;}catch(...){ }return FF_SUCCESS;}if(i==P_REFRESH_FONTS&&x){enumerateFonts();return FF_SUCCESS;}if(i==P_FONT_LIST&&!fonts_.empty()){unsigned n=std::min<unsigned>(unsigned(x+.5f),unsigned(fonts_.size()-1));std::lock_guard<std::mutex>lk(g.mutex);g.font=fonts_[n];fontName_=g.font;g.styleRevision++;return FF_SUCCESS;}std::lock_guard<std::mutex>lk(g.mutex);g.values[i]=x;if(i==P_SHOW_PREVIEW){g.preview=x>.5f;g.cueRevision++;}else g.styleRevision++;return FF_SUCCESS;}
float BRLirik::GetFloatParameter(unsigned i){if(i==P_ROLE)return role_;std::lock_guard<std::mutex>lk(g.mutex);return i<P_COUNT?g.values[i]:0;}
FFResult BRLirik::SetTextParameter(unsigned i,const char*x){std::string s=x?x:"";if(i==P_GOTO_NUMBER){gotoText_=s;return FF_SUCCESS;}if(i==P_FONT_NAME){fontName_=s.empty()?"Arial":s;std::lock_guard<std::mutex>lk(g.mutex);g.font=fontName_;g.styleRevision++;return FF_SUCCESS;}if(i==P_CREDIT||i==P_STATUS)return FF_SUCCESS;return FF_FAIL;}
char* BRLirik::GetTextParameter(unsigned i){static thread_local char b[8192]{};std::string s;if(i==P_CREDIT)s=credit_;else if(i==P_GOTO_NUMBER)s=gotoText_;else if(i==P_FONT_NAME){std::lock_guard<std::mutex>lk(g.mutex);s=g.font;}else if(i==P_STATUS){std::lock_guard<std::mutex>lk(g.mutex);size_t program=g.cues.empty()?0:g.current+1,next=g.cues.empty()?0:std::min(g.current+2,g.cues.size());s=(role_>.5f?"NEXT ":"PROGRAM ")+std::to_string(role_>.5f?next:program)+" / "+std::to_string(g.cues.size());}size_t n=std::min(s.size(),sizeof(b)-1);memcpy(b,s.data(),n);b[n]=0;return b;}
