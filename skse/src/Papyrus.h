#pragma once

// The Papyrus API (script IRF, all global natives). Reads answer at once; changes are queued as one SKSE task.
// Whole numbers only (DevBench and many callers pass ints).
namespace Papyrus
{
	bool Register(RE::BSScript::IVirtualMachine* a_vm);
}
