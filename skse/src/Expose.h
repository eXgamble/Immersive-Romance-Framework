#pragma once

// Two partners, and it comes out in front of both (one greets you as hers where the other hears it, or one sees you
// kiss or date the other): the two of them have it out, then one asks you if it's true (user, 2026-10-26).
//   1 A (who heard or saw it) walks up to B         4 B asks you: "Is this true?" (she comes up to you)
//   2 A: "Who are you to them?"                     5 you: side with A (Persuade), side with B (Persuade), "It's true."
//   3 B claims you (by her stage), A answers (hers)  6 B answers in the talk, A's line after it, and what follows
// Every line is the add-ons' (each candidate's own words in either role); the framework only runs the scene, so it
// works for any two candidates. Without both add-ons' lines, or when something breaks it off (a fight, someone
// leaves, a load), it falls back to both simply catching you.
namespace Expose
{
	// B gave you away, A heard or saw it. Starts the scene when both add-ons have its lines; false when they don't
	// (the caller falls back to both catching you)
	bool Start(RE::Actor* a_b, RE::Actor* a_a);
	bool Running();

	void OnArrived();                                                   // Papyrus: A has walked up to B
	void OnLineBegan(RE::Actor* a_speaker, const std::string& a_key);  // an "expose.*" line started
	void OnLineEnded(RE::Actor* a_speaker, const std::string& a_key);  // an "expose.*" line finished
	void OnAnswered(RE::Actor* a_speaker, const std::string& a_key);   // B's answer to yours ("expose.reply.*")
	void OnDialogueClosed();
	void Check();  // says a line again that didn't start, or breaks the scene off when it can't go on (run on any
	               // candidate's line, a look, a place change, a closed talk)
	void OnGameLoaded();
}
