// ZiiNAN: The GPU oracle owns synthetic world anchors, not a network/Actor/Guild application.
// Fail closed if an untested application service is reached; real Actor setup is tested in the client fixture.
#include "UserInterface/StdAfx.h"
#include "UserInterface/PythonCharacterManager.h"
#include "UserInterface/PythonGuild.h"
#include "UserInterface/MarkManager.h"
#include <stdexcept>
static void UnexpectedFloatingService() { throw std::logic_error("Floating GPU oracle reached an application-only service"); }
CResource* DefaultFont_GetResource() { UnexpectedFloatingService(); return nullptr; }
const char* CInstanceBase::GetNameString() { UnexpectedFloatingService(); return nullptr; }
DWORD CInstanceBase::GetGuildID() { UnexpectedFloatingService(); return 0; }
bool CInstanceBase::CanPickInstance() { UnexpectedFloatingService(); return false; }
BOOL CInstanceBase::IsGuildWall() { UnexpectedFloatingService(); return FALSE; }
void CInstanceBase::NEW_GetPixelPosition(TPixelPosition*) { UnexpectedFloatingService(); }
CActorInstance& CInstanceBase::GetGraphicThingInstanceRef() { throw std::logic_error("No Actor in floating GPU oracle"); }
CActorInstance* CInstanceBase::GetGraphicThingInstancePtr() { UnexpectedFloatingService(); return nullptr; }
CInstanceBase* CPythonCharacterManager::GetMainInstancePtr() { UnexpectedFloatingService(); return nullptr; }
bool CPythonGuild::GetGuildName(DWORD,std::string*) { UnexpectedFloatingService(); return false; }
bool CGuildMarkManager::GetMarkImageFilename(DWORD,std::string&) const { UnexpectedFloatingService(); return false; }
DWORD CGuildMarkManager::GetMarkID(DWORD) { UnexpectedFloatingService(); return 0; }
