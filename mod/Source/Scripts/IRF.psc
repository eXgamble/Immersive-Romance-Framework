Scriptname IRF Hidden
{Immersive Romance Framework: the Papyrus API for add-ons and other mods. All functions are global natives
implemented by ImmersiveRomanceFramework.dll. Changes are carried out by the framework a moment later (on its
own safe point), so read a value back on a later frame, not on the same line.

Stages: -1 not a Friend yet, 0 Friend, 1 Courting, 2 Dating, 3 Committed, 4 Engaged, 5 Married.
On every stage change the framework sends the mod event "IRF_StageChanged" (sender = the candidate,
numArg = the new stage, strArg = the cause).

This file is under the MIT license (see the framework's LICENSE): compile your own scripts against it under
any license.}

; True when an add-on registered this actor as a candidate
bool Function IsCandidate(Actor akActor) global native

; The candidate's stage, or -1 when not a candidate
int Function GetStage(Actor akActor) global native

; The candidate's approval 0..100, or -1 when not a candidate
int Function GetApproval(Actor akActor) global native

; Move a candidate to a stage for a story beat; the framework carries it out (greetings, event, gates).
; asCause is reported in the stage-change event, e.g. "quest".
Function RequestStage(Actor akActor, int aiStage, string asCause = "quest") global native

; Raise or lower approval, with a cause for the log and the hooks, e.g. ModApproval(Camilla, 10, "helped her family")
Function ModApproval(Actor akActor, int aiAmount, string asCause = "script") global native
