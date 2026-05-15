#pragma once
#include "prelude.h"
#include "rpt_slang.h"

#include <stdio.h>
#include <SDL3/SDL.h>

void CSlangCompiler::initialize() {
	createGlobalSession(m_globalSession.writeRef());

	slang::TargetDesc targetDesc = {
		.format {SLANG_SPIRV},
		.profile {m_globalSession->findProfile("spirv_1_6")}
	};

	constexpr u32 preprocessorMacroDescCount = 0;
	// slang::PreprocessorMacroDesc preprocessorMacroDesc[] = {};
	slang::PreprocessorMacroDesc* preprocessorMacroDesc = nullptr;

	constexpr u32 compilerOptionCount = 2;
	slang::CompilerOptionEntry options[compilerOptionCount] = {
		{
			slang::CompilerOptionName::MatrixLayoutColumn,
			{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
		},
		{
			slang::CompilerOptionName::EmitSpirvDirectly,
			{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
		}
	};

	slang::SessionDesc sessionDesc = {
		.targets {&targetDesc},
		.targetCount {1},
		.preprocessorMacros {preprocessorMacroDesc},
		.preprocessorMacroCount {preprocessorMacroDescCount},
		.compilerOptionEntries {options},
		.compilerOptionEntryCount {compilerOptionCount}
	};

	m_globalSession->createSession(sessionDesc, m_localSession.writeRef());
}

void CSlangCompiler::load_module(const char* moduleName, const char* modulePath) {
    FILE* fShaderCode = fopen(modulePath, "r");
    Assert_(fShaderCode);
    usize shaderCodeSize = 0;
    while(!feof(fShaderCode)) {
        u8 c = fgetc(fShaderCode);
        ++shaderCodeSize;
    }
    fclose(fShaderCode);
    fShaderCode = fopen(modulePath, "r"); // crazy inefficient just for testing
    Assert_(fShaderCode);

    char* shaderCode = CSDLAllocator<char>{}.allocate(shaderCodeSize);
    fread(shaderCode, sizeof(u8), shaderCodeSize, fShaderCode);
    fclose(fShaderCode);

	Slang::ComPtr<slang::IModule> slangModule;
	Slang::ComPtr<slang::IBlob> diagnosticBlob;
	slangModule = m_localSession->loadModuleFromSourceString(
		moduleName,
		modulePath,
		shaderCode,
		diagnosticBlob.writeRef());
	log_diagnostic(diagnosticBlob);
	assert(slangModule);
	assert(m_modules.try_add(Sym_(moduleName), slangModule));
	
	CSDLAllocator<char>{}.deallocate(shaderCode, shaderCodeSize);
}

void CSlangCompiler::log_diagnostic(slang::IBlob* diagnosticBlob) {
	if (diagnosticBlob == nullptr) return;

	const char* blobStr = static_cast<const char*>(diagnosticBlob->getBufferPointer());
	SDL_LogError(0, blobStr);
}