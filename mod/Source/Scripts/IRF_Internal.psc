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

; She didn't come along on a fast travel: back where her day has her
Function SendHome(Actor akSpouse) global
	akSpouse.EvaluatePackage()
	akSpouse.MoveToPackageLocation()
EndFunction
