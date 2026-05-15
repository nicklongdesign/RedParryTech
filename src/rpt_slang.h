#pragma once
#include "prelude.h"
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>
#include "rpt_symbol.h"
#include "rpt_allocators.h"
#include "rpt_integral_map.h"

#define TextureModule_ "TextureModule"
#define TextureVertexEntryPoint_ "vMain"
#define TextureFragmentEntryPoint_ "fMain"

class CSlangCompiler {
public:
	CSlangCompiler() {}
	void initialize();

	void load_module(const char* moduleName, const char* modulePath);
	void compile_shader(const char* moduleName, const char* entryPoint);
	const char* get_shader_code(const char* entryPoint, usize& outShaderSize);

	static i32 thread_initialize(void* inSelf) {
		CSlangCompiler* self = Recast_<CSlangCompiler*>(inSelf);
		self->initialize();
		return 0;
	}

protected:
	Slang::ComPtr<slang::IGlobalSession> m_globalSession;
	Slang::ComPtr<slang::ISession> m_localSession;
	TIntegralMap<symbol, Slang::ComPtr<slang::IModule>> m_modules;
	TIntegralMap<symbol, Slang::ComPtr<slang::IBlob>> m_shaders;

	void log_diagnostic(slang::IBlob* diagnosticBlod);
};
