#pragma once

#include <FFGLSDK.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class BRLirik final : public CFFGLPlugin {
public:
  BRLirik();
  ~BRLirik() override;
  FFResult InitGL(const FFGLViewportStruct*) override;
  FFResult ProcessOpenGL(ProcessOpenGLStruct*) override;
  FFResult DeInitGL() override;
  FFResult SetFloatParameter(unsigned int,float) override;
  float GetFloatParameter(unsigned int) override;
  FFResult SetTextParameter(unsigned int,const char*) override;
  char* GetTextParameter(unsigned int) override;
  static LRESULT CALLBACK EditorWndProc(HWND,UINT,WPARAM,LPARAM);

private:
  void addOption(unsigned int,unsigned int,const char*,float);
  void enumerateFonts();
  void openEditor();
  void runEditor();
  void rebuildTextures(bool);
  unsigned int buildFrameTexture(const std::string&,bool,unsigned int&,unsigned int&);
  void releaseTextures();
  std::string cueForRole(bool previous=false) const;

  std::vector<std::string> fonts_;
  std::string fontName_="Arial";
  std::string gotoText_="1";
  std::string currentCue_,previousCue_;
  const std::string credit_="Effect by Belajar Resolume";
  float role_=0.0f;
  unsigned int viewportWidth_=1920,viewportHeight_=1080;
  unsigned int currentTexture_=0,previousTexture_=0;
  unsigned int textureWidth_=1,textureHeight_=1;
  unsigned long long seenRevision_=0,seenStyleRevision_=0;
  bool transitionActive_=false;
  std::chrono::steady_clock::time_point transitionStarted_,lastFrame_;
  double elapsed_=0.0;
  unsigned long long gdiplusToken_=0;

  std::atomic<HWND> editorWindow_{nullptr};
  HWND editorTextBox_=nullptr;
  std::atomic<bool> editorRunning_{false},editorStopRequested_{false};
  std::thread editorThread_;
  ffglex::FFGLShader shader_;
  ffglex::FFGLScreenQuad quad_;
};
