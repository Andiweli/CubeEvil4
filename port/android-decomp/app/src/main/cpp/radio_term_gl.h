#pragma once
#include "radio_term.h"
namespace re4 {
void CreateRadioTermGl(const RadioTermRuntime &runtime);
void DrawRadioTermGl(int width,int height,const RadioTermRuntime &runtime);
void DestroyRadioTermGl();
void ForgetRadioTermGl();
}
