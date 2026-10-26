Scriptname IRF_Internal Hidden
{Immersive Romance Framework: helpers the framework's DLL calls. Not an API; use IRF instead.}

; Say a topic after a moment of game time (a line said while a dialogue is still closing never plays)
Function SayLater(ObjectReference akSpeaker, Topic akTopic, float afSeconds) global
	Utility.Wait(afSeconds)
	If akSpeaker && !(akSpeaker as Actor).IsDead()
		akSpeaker.Say(akTopic)
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

; Two partners have it out (the framework's Expose). IRF_SayQuest's aliases 1 (ExposeHoldA) and 2 (ExposeHoldB) hold
; them with vanilla's DoNothing package, so their day doesn't take them off mid-scene. A walks over to B; the framework
; hears when she's there
Function ExposeApproach(Quest akQuest, Actor akA, Actor akB) global
	ExposeHold(akQuest, 2, akB)
	ExposeHold(akQuest, 1, akA)
	akA.SetLookAt(akB)
	akB.SetLookAt(akA)
	If akA.GetDistance(akB) > 170.0
		akA.PathToReference(akB, 0.5)  ; a scripted walk: it overrides the hold while it lasts
	EndIf
	ExposeArrived()
EndFunction

; Holds an actor in hold alias aiAlias (1 A, 2 B), or lets it go (akActor None)
Function ExposeHold(Quest akQuest, int aiAlias, Actor akActor) global
	ReferenceAlias hold = akQuest.GetAlias(aiAlias) as ReferenceAlias
	If !hold
		Debug.Trace("IRF: IRF_SayQuest has no hold alias " + aiAlias + " (a save from before it)")
		Return
	EndIf
	Actor was = hold.GetActorReference()
	If akActor
		hold.ForceRefTo(akActor)
		akActor.EvaluatePackage()
	Else
		hold.Clear()
		If was
			was.EvaluatePackage()
		EndIf
	EndIf
EndFunction

; The scene is over: both go back to their day
Function ExposeEnd(Quest akQuest, Actor akA, Actor akB) global
	akA.ClearLookAt()
	akB.ClearLookAt()
	ExposeHold(akQuest, 1, None)
	ExposeHold(akQuest, 2, None)
EndFunction

Function ExposeArrived() global native

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
