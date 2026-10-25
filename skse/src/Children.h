#pragma once

#include "Types.h"

// Adopted children (Hearthfire's adoption, unchanged), married to a candidate:
// - Their greeting tells you when something is wrong at home: you were away long (IRF_ChildFaction 1-3), or the
//   marriage is strained, bad or heartbroken (11-13, once a day). All is well: vanilla's greeting. The lines are the
//   framework's own (IRF_ChildHelloTopic, both child voices), never naming a parent.
// - The spouse stays home with them: she follows you inside the family's town; at its edge she comes up to you
//   (IRF_StayHomeForceGreet), says why (her add-on's "stayhome" line) and goes home. A fast travel can't play a line,
//   so a message says it instead.
namespace Children
{
	// Hearthfire adopted this actor (BYOHRelationshipAdoptionFaction)
	bool IsChild(RE::Actor* a_actor);

	// How many boys and girls Hearthfire has you raising; Count() writes them to IRF_AdoptedBoys and IRF_AdoptedGirls
	// (any line can read them) when they changed: after a load, when you look at someone or open a conversation
	int  Boys();
	int  Girls();
	void Count();

	// Up close (crosshair): the greeting they'd give now
	void Sync(RE::Actor* a_child);

	// One of the framework's child greetings was said, or a talk with them ended: they've seen you
	void OnGreeted(RE::Actor* a_child);
	void OnTalked(RE::Actor* a_child);

	// The player's location changed / a loading screen closed / a fast travel ended
	void OnPlayerLocation(RE::BGSLocation* a_location);
	void OnLoadingDone();
	void OnFastTravelEnd();

	// The spouse finished her "stayhome" line: she goes home
	void OnStayHomeSaid(RE::Actor* a_spouse);

	void OnGameLoaded();
}
