Scriptname IRF Hidden
{Immersive Romance Framework: the Papyrus API for add-ons and other mods. All functions are global natives
implemented by ImmersiveRomanceFramework.dll. Changes are carried out by the framework a moment later (on its
own safe point), so read a value back on a later frame, not on the same line.

Stages: -1 not a Friend yet, 0 Friend, 1 Courting, 2 Dating, 3 Committed, 4 Engaged, 5 Married, 6 Ex, 7 Widowed,
8 Killed (you killed them and someone saw it).
Why (a breakup): 1 cheating, 2 violence, 3 absence, 4 bleedouts, 5 crime, 6 the player's choice.
Who (ended it): 1 the player, 2 the candidate.

Mod events:
  "IRF_StageChanged"  sender = the candidate, numArg = the new stage, strArg = the cause
  "IRF_Strayed"       sender = the partner strayed from, numArg = 1 if caught, strArg = the deed

This file is under the MIT license (see the framework's LICENSE): compile your own scripts against it under
any license.}

; True when an add-on registered this actor as a candidate
bool Function IsCandidate(Actor akActor) global native

; The candidate's stage, or -1 when not a candidate
int Function GetStage(Actor akActor) global native

; The candidate's approval 0..100, or -1 when not a candidate
int Function GetApproval(Actor akActor) global native

; The player's partner (Dating or later, married included), or None
Actor Function GetPartner() global native

; Why and who of the candidate's last breakup (0 when none)
int Function GetWhy(Actor akActor) global native
int Function GetWho(Actor akActor) global native

; True while the candidate is heartbroken (silent days, or hurt until amends)
bool Function IsBrokenHearted(Actor akActor) global native

; "Would they, here?" for mods that start scenes (OStim and the like): aiLevel 0 = a kiss, 1 = an intimate scene.
; Answers from the candidate's stage, heartbreak and modesty (privacy of the place, people watching). True for
; anyone who is not a candidate (not the framework's to judge).
bool Function IsIntimacyAllowed(Actor akActor, int aiLevel = 0) global native

; Move a candidate to a stage for a story beat; the framework carries it out (greetings, event, gates).
Function RequestStage(Actor akActor, int aiStage, string asCause = "quest") global native

; Raise or lower approval, with a cause for the log and the hooks, e.g. ModApproval(Camilla, 10, "helped her family")
Function ModApproval(Actor akActor, int aiAmount, string asCause = "script") global native

; Who the player is interested in: "any", "male" or "female" (overrides Rules.json; eXrim's character creator)
Function SetPlayerInterest(string asInterest) global native

; How many boys and girls Hearthfire has you raising (also in the globals IRF_AdoptedBoys and IRF_AdoptedGirls)
int Function GetAdoptedBoys() global native
int Function GetAdoptedGirls() global native

; ---- debug tools ----
; Set approval directly
Function DebugSetApproval(Actor akActor, int aiValue) global native
; Move this candidate's clocks back as if aiDays had passed (absence, broken heart, win-back, refusals)
Function DebugSkipDays(Actor akActor, int aiDays) global native
; Trigger a deed: "caught", "bleedout", "attack", "absence", "crime", "breakup", "amends", "xp"
Function DebugDeed(Actor akActor, string asDeed) global native
; Write everything the framework knows about the candidate to ImmersiveRomanceFramework.log (and a notification)
Function DebugDump(Actor akActor) global native
; Forget the candidate's relationship entirely
Function DebugReset(Actor akActor) global native

; A child counts as adopted (Hearthfire's child alias and adoption faction) without Hearthfire's adoption and move
Function DebugAdopt(Actor akChild) global native
