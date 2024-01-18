#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <regex>
#include <cstdint>
#include <iostream>

#include "pch.h"
#include "SNES/Debugger/SnesDisUtils.h"
#include "Shared/Emulator.h"
#include "Shared/KeyManager.h"
#include "Shared/DebuggerRequest.h"
#include "SNES/SnesConsole.h"
#include "Debugger/Debugger.h"
#include "Debugger/CdlManager.h"
#include "Debugger/LabelManager.h"
#include "Debugger/Disassembler.h"
#include "Utilities/VirtualFile.h"
#include "Shared/Interfaces/IConsole.h"
#include "Utilities/FolderUtilities.h"
#include "Core/Shared/EmuSettings.h"

using namespace std;

struct CodeLabel {
    uint32_t Address;
    MemoryType MemoryType;
    std::string Label;
    uint32_t Length;
    std::string Comment;
};

bool GetLegacyMemoryType(const std::string& name, MemoryType& type) {
    if (name == "SnesRegister") {
        type = MemoryType::SnesRegister;
    } else if (name == "SnesPrgRom") {
        type = MemoryType::SnesPrgRom;
    } else if (name == "SAVE") {
        type = MemoryType::SnesSaveRam;
    } else if (name == "SnesWorkRam") {
        type = MemoryType::SnesWorkRam;
    } else if (name == "IRAM") {
        type = MemoryType::Sa1InternalRam;
    } else if (name == "SpcRam") {
        type = MemoryType::SpcRam;
    } else if (name == "SPCROM") {
        type = MemoryType::SpcRom;
    } else {
        return false;
    }

    return true;
}

bool TryParseHex(const std::string& str, uint32_t& outValue) {
    std::stringstream ss;
    ss << std::hex << str;
    ss >> outValue;
    return !ss.fail();
}

std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::stringstream ss(str);
    std::string item;
    std::vector<std::string> tokens;
    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }
    return tokens;
}

bool IsValidLabel(const std::string& label) {
    // Define a regular expression for label validation
    std::regex labelRegex("^[@_a-zA-Z]+[@_a-zA-Z0-9]*$");
    return std::regex_match(label, labelRegex);
}

std::unique_ptr<CodeLabel> FromString(const std::string& data) {
    const char _separator = ':'; 
    std::vector<std::string> rowData = SplitString(data, _separator);
    if (rowData.size() < 3) {
			cerr << "Invalid size" << endl;
        return nullptr;
    }

    MemoryType type;
    if (!GetLegacyMemoryType(rowData[0], type)) {
			cerr << "Invalid memory type " << rowData[0] << endl;
        return nullptr;
    }

    std::string addressString = rowData[1];
    uint32_t address = 0;
    uint32_t length = 1;
    if (addressString.find('-') != std::string::npos) {
        std::vector<std::string> addressStartEnd = SplitString(addressString, '-');
        uint32_t addressEnd;
        if (TryParseHex(addressStartEnd[0], address) && TryParseHex(addressStartEnd[1], addressEnd)) {
            if (addressEnd > address) {
                length = addressEnd - address + 1;
            } else {
					cerr << "TryParseHex 1" << endl;
               return nullptr;
            }
        } else {
            return nullptr;
        }
    } else {
        if (!TryParseHex(rowData[1], address)) {
				cerr << "TryParseHex 2" << endl;
            return nullptr;
        }
    }

    std::string labelName = rowData[2];
    if (!labelName.empty() && !IsValidLabel(labelName)) {
		cerr << "Invalid label" << endl;
        return nullptr;
    }

    auto codeLabel = std::make_unique<CodeLabel>();
    codeLabel->Address = address;
    codeLabel->MemoryType = type;
    codeLabel->Label = labelName;
    codeLabel->Length = length;
    codeLabel->Comment = "";

    if (rowData.size() > 3) {
        codeLabel->Comment = rowData[3];  // Note: Replace "\\n" with "\n" if needed
    }

    return codeLabel;
}

std::vector<std::unique_ptr<CodeLabel>> ReadCodeLabelsFromFile(const std::string& filename) {
    std::vector<std::unique_ptr<CodeLabel>> codeLabels;
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return codeLabels;
    }

	 // Check for UTF-8 BOM and skip it
    char bom[3] = {0};
    file.read(bom, 3);
    if (bom[0] != (char)0xEF || bom[1] != (char)0xBB || bom[2] != (char)0xBF) {
        // No BOM, so rewind to the start of the file
        file.seekg(0);
    }


    while (std::getline(file, line)) {
        auto codeLabel = FromString(line);
        if (codeLabel) {
            codeLabels.push_back(std::move(codeLabel));
        }
    }

    file.close();
    return codeLabels;
}

int main(int argc, char *argv[])
{
	FolderUtilities::SetHomeFolder("/Users/lucas/");

	auto labels = ReadCodeLabelsFromFile("/Users/lucas/_dev/reversanigma/Terranigma.mlb");
	
	auto emu = Emulator();
	auto _emu = &emu;
	_emu->Initialize();
	KeyManager::SetSettings(_emu->GetSettings());

	auto romFile = VirtualFile("/Users/lucas/_dev/reversanigma/rom/Terranigma.sfc");
	emu.LoadRom(romFile, VirtualFile());

	//_emu->GetSettings()->SetFlag(EmulationFlags::MaximumSpeed);
	//std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(250));
	std::cerr << "Ran for " << _emu->GetFrameCount() << " frames" << std::endl;

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


	for (size_t i = 0; i < labels.size(); i++)
	{
		cerr << labels[i]->Address << " " << labels[i]->Label << endl;
		debugger->GetLabelManager()->SetLabel(
			labels[i]->Address,
			labels[i]->MemoryType,
			labels[i]->Label,
			labels[i]->Comment
		);
	}

	//CodeLineData foo[100];

	auto memory = _emu->GetMemory(MemoryType::SnesPrgRom);
	vector<CodeLineData> foo(memory.Size);
	
	//disassembler->GetDisassemblyOutput(_emu->GetCpuTypes()[0], 0x8000, foo, 100);
	disassembler->GetDisassemblyOutput(_emu->GetCpuTypes()[0], 0, foo.data(), memory.Size);

	cout << "[" << endl;
	for (size_t i = 0; i < memory.Size; i++)
	{
		if(foo[i].Address == 0) {
			continue;
		}

		cout << "{ ";
		cout << '"' << "address" << "\": \"" << foo[i].Address << "\", ";
		//cout << '"' << "byteCode" << "\": \"" << foo[i].ByteCode << "\",";
		cout << '"' << "comment" << "\": \"" << foo[i].Comment << "\", ";
		cout << '"' << "flags" << "\": \"" << foo[i].Flags << "\", ";
		// cout << '"' << "value" << "\": \"" << foo[i].Value << "\", ";
		cout << '"' << "text" << "\": \"" << foo[i].Text << "\"";
		cout << "}," << endl;
	}
	cout << "{}]" << endl;
	
	_emu->Stop(false);
	_emu->Release();
}
