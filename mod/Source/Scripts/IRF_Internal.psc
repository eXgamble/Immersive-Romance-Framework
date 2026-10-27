Scriptname IRF_Internal Hidden
{Immersive Romance Framework: helpers the framework's DLL calls. Not an API; use IRF instead.}

; Say a topic after a moment of game time (a line said while a dialogue is still closing never plays)
Function SayLater(ObjectReference akSpeaker, Topic akTopic, float afSeconds) global
	Utility.Wait(afSeconds)
	If akSpeaker && !(akSpeaker as Actor).IsDead()
		akSpeaker.Say(akTopic)
	EndIf
EndFunction

; Her idles rest after one of them (the DLL set akRest to 1, which her idle lines need to be anything but): back on
; after afSeconds of play
Function RestIdles(Actor akActor, Faction akRest, float afSeconds) global
	Utility.Wait(afSeconds)
	If akActor && akActor.GetFactionRank(akRest) == 1
		akActor.SetFactionRank(akRest, 0)
	EndIf
EndFunction

; Someone coming up to tell you something (IRF_MomentFaction says what: a spouse who stays home with the children,
; a confession): IRF_SayQuest's ForceGreet alias (alias 0, its force greet) on or off
Function ForceGreet(Quest akQuest, Actor akSpouse, bool abOn) global
	ReferenceAlias home = akQuest.GetAlias(0) as ReferenceAlias
	If abOn
		home.ForceRefTo(akSpouse)
	Else
		home.Clear()
	EndIf
	akSpouse.EvaluatePackage()
EndFunction

; A scene between two of them (the framework's: two partners having it out, a family conversation). IRF_SayQuest's
; aliases 1 and 2 hold them with vanilla's DoNothing package, so their day doesn't take them off mid-scene. The walker
; walks over to the other; the framework hears when she's there (aiScene: 1 Expose, 2 Family)
Function Approach(Quest akQuest, Actor akWalker, Actor akTarget, int aiScene) global
	Hold(akQuest, 2, akTarget)
	Hold(akQuest, 1, akWalker)
	akWalker.SetLookAt(akTarget)
	akTarget.SetLookAt(akWalker)
	If akWalker.GetDistance(akTarget) > 170.0
		akWalker.PathToReference(akTarget, 0.5)  ; a scripted walk: it overrides the hold while it lasts
	EndIf
	Arrived(aiScene)
EndFunction

; Holds an actor in hold alias aiAlias (1 or 2), or lets it go (akActor None)
Function Hold(Quest akQuest, int aiAlias, Actor akActor) global
	ReferenceAlias slot = akQuest.GetAlias(aiAlias) as ReferenceAlias
	If !slot
		Debug.Trace("IRF: IRF_SayQuest has no hold alias " + aiAlias + " (a save from before it)")
		Return
	EndIf
	Actor was = slot.GetActorReference()
	If akActor
		slot.ForceRefTo(akActor)
		akActor.EvaluatePackage()
	Else
		slot.Clear()
		If was
			was.EvaluatePackage()
		EndIf
	EndIf
EndFunction

; The scene is over: both go back to their day
Function Release(Quest akQuest, Actor akA, Actor akB) global
	akA.ClearLookAt()
	akB.ClearLookAt()
	Hold(akQuest, 1, None)
	Hold(akQuest, 2, None)
EndFunction

Function Arrived(int aiScene) global native

; The scene director's clock while a scene is on: a beat afSeconds later (Wait doesn't count menu time, so a
; loading screen holds it back)
Function BeatLater(int aiBeat, float afSeconds) global
	Utility.Wait(afSeconds)
	Beat(aiBeat)
EndFunction

Function Beat(int aiBeat) global native

; She didn't come along on a fast travel: back where her day has her
Function SendHome(Actor akSpouse) global
	akSpouse.EvaluatePackage()
	akSpouse.MoveToPackageLocation()
EndFunction

; The first date is set: IRF_FirstDateQuest holds her (alias 0, her wait package takes her to her spot), her spot
; (alias 1) and its place for the journal (alias 2), and shows "Meet her at the place after sunset"
Function StartDate(Quest akQuest, Actor akDate, ObjectReference akSpot, Location akPlace) global
	If !akQuest.IsRunning()
		akQuest.Start()
	EndIf
	(akQuest.GetAlias(0) as ReferenceAlias).ForceRefTo(akDate)
	(akQuest.GetAlias(1) as ReferenceAlias).ForceRefTo(akSpot)
	If akPlace
		(akQuest.GetAlias(2) as LocationAlias).ForceLocationTo(akPlace)
	EndIf
	akQuest.SetObjectiveDisplayed(10)
	akDate.EvaluatePackage()
EndFunction

; The date is over: kept (the objective is done) or missed (failed); she goes back to her day
Function EndDate(Quest akQuest, Actor akDate, bool abKept) global
	If abKept
		akQuest.SetObjectiveCompleted(10)
	Else
		akQuest.SetObjectiveFailed(10)
	EndIf
	akQuest.Stop()
	akDate.EvaluatePackage()
EndFunction
