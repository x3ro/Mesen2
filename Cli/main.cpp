#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>

#include "pch.h"
#include "SNES/Debugger/SnesDisUtils.h"
#include "Shared/Emulator.h"
#include "Shared/KeyManager.h"
#include "Shared/DebuggerRequest.h"
#include "SNES/SnesConsole.h"
#include "Debugger/Debugger.h"
#include "Debugger/CdlManager.h"
#include "Debugger/Disassembler.h"
#include "Utilities/VirtualFile.h"
#include "Shared/Interfaces/IConsole.h"
#include "Utilities/FolderUtilities.h"
#include "Core/Shared/EmuSettings.h"

using namespace std;

int main(int argc, char *argv[])
{
	FolderUtilities::SetHomeFolder("/Users/lucas/");
	
	auto emu = Emulator();
	auto _emu = &emu;
	_emu->Initialize();
	KeyManager::SetSettings(_emu->GetSettings());

	auto romFile = VirtualFile("/Users/lucas/_dev/reversanigma/rom/Terranigma.sfc");
	emu.LoadRom(romFile, VirtualFile());

	//_emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
	//std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(250));
	std::cout << "Ran for " << _emu->GetFrameCount() << " frames" << std::endl;

	auto debugger = _emu->GetDebugger(true).GetDebugger();
	if(!debugger) {
		cerr << "Debugger uninitialized" << endl;
		exit(1);
	}

	debugger->GetCdlManager()->LoadCdlFile(MemoryType::SnesPrgRom, "/Users/lucas/.config/Mesen2/Debugger/Terranigma.cdl");

	auto disassembler = debugger->GetDisassembler();
	if(!disassembler) {
		cerr << "disassembler uninitialized" << endl;
		exit(1);
	}
	
	CodeLineData foo[100];
	disassembler->GetDisassemblyOutput(_emu->GetCpuTypes()[0], 0x8000, foo, 100);

	for (size_t i = 0; i < 100; i++)
	{
		cout << foo[i].Text << endl;
	}
	
	_emu->Stop(false);
	_emu->Release();
}
