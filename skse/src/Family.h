#pragma once

// Family conversations: married to a candidate, with adopted children, at home she and a child talk where you can
// hear it (user, 2026-10-26). The conversations are her add-on's own ("family": both halves, any number of turns,
// in either order), picked by how things are at home (her heart and approval: happy, fine, strained, bad,
// heartbroken) and her mood. The framework only stages them: once a game day, the one who speaks first walks over,
// both are held for it, the lines play in turn. A fight, someone leaving or a load ends it quietly.
namespace Family
{
	// A moment the family may talk (you arrived somewhere, looked at someone, a loading screen closed)
	void Check();
	bool Running();

	void OnArrived();                                                   // Papyrus: the first speaker walked over
	void OnLineBegan(RE::Actor* a_speaker, const std::string& a_key);  // a "family.<n>.<step>" line started
	void OnLineEnded(RE::Actor* a_speaker, const std::string& a_key);  // ... and finished
	void OnDialogueClosed();
	void Tick();  // says a line again that didn't start, or ends a conversation that can't go on (run on any line)
	void OnGameLoaded();
}
